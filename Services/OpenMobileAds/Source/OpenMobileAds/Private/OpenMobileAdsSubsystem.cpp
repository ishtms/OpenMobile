#include "OpenMobileAdsSubsystem.h"

#include "Async/Async.h"
#include "Features/IModularFeatures.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobileAdsConsentSignalConsumer.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileAdsCanRequestPolicy.h"
#include "OpenMobileAdsCanShowPolicy.h"
#include "OpenMobileAdsClock.h"
#include "OpenMobileAdsConnectivityPolicy.h"
#include "OpenMobileAdsCooldown.h"
#include "OpenMobileAdsDiagnostics.h"
#include "OpenMobileAdsFrequencyCap.h"
#include "OpenMobileAdsFullscreenLifecycle.h"
#include "OpenMobileAdsRetry.h"
#include "OpenMobileAdsTrackingAuthorizationPlatform.h"

class FOpenMobileAdsEventDispatcher final
	: public TSharedFromThis<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe>
{
public:
	explicit FOpenMobileAdsEventDispatcher(UOpenMobileAdsSubsystem& InSubsystem)
		: Subsystem(&InSubsystem)
	{
	}

	void Submit(FOpenMobileAdsEvent Event)
	{
		bool bScheduleDrain = false;
		{
			FScopeLock Lock(&Mutex);
			if (!Subsystem.IsValid())
			{
				return;
			}
			Event.Sequence = NextSequence++;
			if (Event.Timestamp == FDateTime())
			{
				Event.Timestamp = FDateTime::UtcNow();
			}
			PendingEvents.Add(MoveTemp(Event));
			if (!bDrainScheduled)
			{
				bDrainScheduled = true;
				bScheduleDrain = true;
			}
		}

		if (bScheduleDrain)
		{
			const TSharedRef<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe> Self =
				AsShared();
			AsyncTask(ENamedThreads::GameThread, [Self]()
			{
				Self->Drain();
			});
		}
	}

	void Invalidate()
	{
		FScopeLock Lock(&Mutex);
		Subsystem.Reset();
		PendingEvents.Reset();
	}

private:
	void Drain()
	{
		check(IsInGameThread());

		TArray<FOpenMobileAdsEvent> Events;
		TWeakObjectPtr<UOpenMobileAdsSubsystem> Target;
		{
			FScopeLock Lock(&Mutex);
			Target = Subsystem;
			Events = MoveTemp(PendingEvents);
			PendingEvents.Reset();
			bDrainScheduled = false;
		}

		for (FOpenMobileAdsEvent& Event : Events)
		{
			if (UOpenMobileAdsSubsystem* SubsystemObject = Target.Get())
			{
				SubsystemObject->HandleProviderEvent(MoveTemp(Event));
			}
		}
	}

	FCriticalSection Mutex;
	TWeakObjectPtr<UOpenMobileAdsSubsystem> Subsystem;
	TArray<FOpenMobileAdsEvent> PendingEvents;
	int64 NextSequence = 1;
	bool bDrainScheduled = false;
};

namespace OpenMobileAdsPrivate
{
	constexpr int32 MaxDismissedShowRewardContexts = 64;

	FOpenMobileAdsConsentSignals MakeConsentSignals(
		const FOpenMobileAdsPrivacySnapshot& Snapshot
	)
	{
		FOpenMobileAdsConsentSignals Signals;
		Signals.ConsentStatus = Snapshot.ConsentStatus;
		Signals.GdprApplicability = Snapshot.GdprApplicability;
		Signals.ConsentRequirement = Snapshot.ConsentRequirement;
		Signals.ConsentRequestState = Snapshot.ConsentRequestState;
		Signals.bConsentStatusFresh =
			Snapshot.IsConsentStatusFreshAt(FDateTime::UtcNow());
		Signals.UsPrivacy = Snapshot.UsPrivacy;
		Signals.ChildDirectedTreatment = Snapshot.ChildDirectedTreatment;
		Signals.UnderAgeOfConsent = Snapshot.UnderAgeOfConsent;
		Signals.Source = Snapshot.Source;
		return Signals;
	}

	FOpenMobileAdsProviderRequestContext MakeProviderPrivacyContext(
		const FOpenMobileAdsPrivacySnapshot& Snapshot
	)
	{
		FOpenMobileAdsProviderRequestContext Context;
		Context.ConsentStatus = Snapshot.ConsentStatus;
		Context.bConsentStatusFresh =
			Snapshot.IsConsentStatusFreshAt(FDateTime::UtcNow());
		Context.ChildDirectedTreatment = Snapshot.ChildDirectedTreatment;
		Context.UnderAgeOfConsent = Snapshot.UnderAgeOfConsent;
		Context.UsPrivacy = Snapshot.UsPrivacy;
		Context.ConsentSignals = MakeConsentSignals(Snapshot);
		return Context;
	}

	class FInitializationSink final : public IOpenMobileAdsProviderInitializationSink
	{
	public:
		FInitializationSink(
			TFunction<void(FOpenMobileAdsInitializationComponentStatus)>&& InStatusUpdate,
			TFunction<void(FOpenMobileAdsError)>&& InCompletion
		)
			: StatusUpdate(MoveTemp(InStatusUpdate))
			, Completion(MoveTemp(InCompletion))
		{
		}

		virtual void UpdateStatus(
			FOpenMobileAdsInitializationComponentStatus Status
		) override
		{
			TFunction<void(FOpenMobileAdsInitializationComponentStatus)> StatusUpdateToRun;
			{
				FScopeLock Lock(&Mutex);
				if (!bValid)
				{
					return;
				}
				if (!bCommitted)
				{
					PendingStatuses.Add(MoveTemp(Status));
					return;
				}
				StatusUpdateToRun = StatusUpdate;
			}
			StatusUpdateToRun(MoveTemp(Status));
		}

		virtual void Complete(FOpenMobileAdsError Error) override
		{
			TFunction<void(FOpenMobileAdsError)> CompletionToRun;
			{
				FScopeLock Lock(&Mutex);
				if (!bValid || bCompletionSubmitted)
				{
					return;
				}
				bCompletionSubmitted = true;
				if (!bCommitted)
				{
					PendingError = MoveTemp(Error);
					bHasPendingCompletion = true;
					return;
				}
				CompletionToRun = Completion;
			}
			CompletionToRun(MoveTemp(Error));
		}

		void Commit()
		{
			TFunction<void(FOpenMobileAdsInitializationComponentStatus)> StatusUpdateToRun;
			TFunction<void(FOpenMobileAdsError)> CompletionToRun;
			TArray<FOpenMobileAdsInitializationComponentStatus> Statuses;
			FOpenMobileAdsError Error;
			bool bRunCompletion = false;
			{
				FScopeLock Lock(&Mutex);
				if (!bValid || bCommitted)
				{
					return;
				}
				bCommitted = true;
				Statuses = MoveTemp(PendingStatuses);
				StatusUpdateToRun = StatusUpdate;
				bRunCompletion = bHasPendingCompletion;
				if (bRunCompletion)
				{
					Error = MoveTemp(PendingError);
				}
				CompletionToRun = Completion;
			}
			for (FOpenMobileAdsInitializationComponentStatus& Status : Statuses)
			{
				StatusUpdateToRun(MoveTemp(Status));
			}
			if (bRunCompletion)
			{
				CompletionToRun(MoveTemp(Error));
			}
		}

		virtual void Invalidate() override
		{
			FScopeLock Lock(&Mutex);
			bValid = false;
			PendingStatuses.Reset();
			bHasPendingCompletion = false;
			StatusUpdate = nullptr;
			Completion = nullptr;
		}

	private:
		FCriticalSection Mutex;
		TFunction<void(FOpenMobileAdsInitializationComponentStatus)> StatusUpdate;
		TFunction<void(FOpenMobileAdsError)> Completion;
		TArray<FOpenMobileAdsInitializationComponentStatus> PendingStatuses;
		FOpenMobileAdsError PendingError;
		bool bHasPendingCompletion = false;
		bool bCommitted = false;
		bool bCompletionSubmitted = false;
		bool bValid = true;
	};

	class FConsentProviderSink final : public IOpenMobileAdsConsentProviderSink
	{
	public:
		FConsentProviderSink(
			TFunction<void(FOpenMobileAdsConsentStatusUpdate)>&& InCompletion,
			TFunction<void(FOpenMobileAdsError)>&& InFailure
		)
			: Completion(MoveTemp(InCompletion))
			, Failure(MoveTemp(InFailure))
		{
		}

		virtual void Complete(FOpenMobileAdsConsentStatusUpdate Update) override
		{
			TFunction<void(FOpenMobileAdsConsentStatusUpdate)> CompletionToRun;
			{
				FScopeLock Lock(&Mutex);
				if (!bValid || bTerminalSubmitted)
				{
					return;
				}
				bTerminalSubmitted = true;
				if (!bCommitted)
				{
					PendingUpdate = MoveTemp(Update);
					bHasPendingUpdate = true;
					return;
				}
				CompletionToRun = Completion;
			}
			CompletionToRun(MoveTemp(Update));
		}

		virtual void Fail(FOpenMobileAdsError Error) override
		{
			TFunction<void(FOpenMobileAdsError)> FailureToRun;
			{
				FScopeLock Lock(&Mutex);
				if (!bValid || bTerminalSubmitted)
				{
					return;
				}
				bTerminalSubmitted = true;
				if (!bCommitted)
				{
					PendingError = MoveTemp(Error);
					bHasPendingError = true;
					return;
				}
				FailureToRun = Failure;
			}
			FailureToRun(MoveTemp(Error));
		}

		void Commit()
		{
			TFunction<void(FOpenMobileAdsConsentStatusUpdate)> CompletionToRun;
			TFunction<void(FOpenMobileAdsError)> FailureToRun;
			FOpenMobileAdsConsentStatusUpdate Update;
			FOpenMobileAdsError Error;
			bool bRunCompletion = false;
			bool bRunFailure = false;
			{
				FScopeLock Lock(&Mutex);
				if (!bValid || bCommitted)
				{
					return;
				}
				bCommitted = true;
				bRunCompletion = bHasPendingUpdate;
				bRunFailure = bHasPendingError;
				if (bRunCompletion)
				{
					Update = MoveTemp(PendingUpdate);
					CompletionToRun = Completion;
				}
				if (bRunFailure)
				{
					Error = MoveTemp(PendingError);
					FailureToRun = Failure;
				}
			}
			if (bRunCompletion)
			{
				CompletionToRun(MoveTemp(Update));
			}
			else if (bRunFailure)
			{
				FailureToRun(MoveTemp(Error));
			}
		}

		virtual void Invalidate() override
		{
			FScopeLock Lock(&Mutex);
			bValid = false;
			bHasPendingUpdate = false;
			bHasPendingError = false;
			Completion = nullptr;
			Failure = nullptr;
		}

	private:
		FCriticalSection Mutex;
		TFunction<void(FOpenMobileAdsConsentStatusUpdate)> Completion;
		TFunction<void(FOpenMobileAdsError)> Failure;
		FOpenMobileAdsConsentStatusUpdate PendingUpdate;
		FOpenMobileAdsError PendingError;
		bool bHasPendingUpdate = false;
		bool bHasPendingError = false;
		bool bCommitted = false;
		bool bTerminalSubmitted = false;
		bool bValid = true;
	};

	FOpenMobileAdsError NormalizeProviderError(
		FOpenMobileAdsError Error,
		EOpenMobileAdsFailureStage Stage,
		FName Placement,
		FName Provider,
		const TCHAR* FallbackExplanation
	)
	{
		const bool bReportedRetryable = Error.bRetryable;
		const FOpenMobileAdsNativeDiagnostics ReportedDiagnostics =
			Error.NativeDiagnostics;
		const bool bHasNativeFailureDetails =
			!ReportedDiagnostics.NativeCode.IsEmpty()
			|| !ReportedDiagnostics.NativeMessage.IsEmpty()
			|| !ReportedDiagnostics.Adapter.IsEmpty();
		if (!Error.IsSet() && bHasNativeFailureDetails)
		{
			FOpenMobileAdsErrorMappingContext Context;
			Context.Domain = Error.NativeDiagnostics.Adapter.IsEmpty()
				? EOpenMobileAdsErrorDomain::Provider
				: EOpenMobileAdsErrorDomain::Mediation;
			Context.Stage = Error.Stage == EOpenMobileAdsFailureStage::None
				? Stage
				: Error.Stage;
			Context.Placement = Placement;
			Context.Provider = Provider;
			Context.Network = Error.NativeDiagnostics.Network;
			Context.Adapter = Error.NativeDiagnostics.Adapter;
			Context.NativeCode = Error.NativeDiagnostics.NativeCode;
			Context.NativeMessage = Error.NativeDiagnostics.NativeMessage;
			Error = FOpenMobileAdsErrorMapper::FromNative(Context);
		}
		else if (!Error.IsSet())
		{
			Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderFailure,
				Stage,
				Placement,
				FallbackExplanation,
				Provider
			);
			Error.NativeDiagnostics = ReportedDiagnostics;
		}
		if (Error.Stage == EOpenMobileAdsFailureStage::None)
		{
			Error.Stage = Stage;
		}
		Error.Placement = Placement;
		Error.Provider = Provider;
		Error.bRetryable |= bReportedRetryable;
		if (Error.Explanation.IsEmpty())
		{
			Error.Explanation = FallbackExplanation;
		}
		if (Error.LikelyCause.IsEmpty())
		{
			Error.LikelyCause = Error.Explanation;
		}
		if (Error.SuggestedCorrection.IsEmpty())
		{
			Error.SuggestedCorrection =
				TEXT("Check the provider diagnostics and retry when the failure is retryable.");
		}
		if (Error.NativeDiagnostics.IsSet())
		{
			Error.NativeDiagnostics.Provider = Provider;
		}
		return Error;
	}

	FOpenMobileAdsError NormalizeConsentError(
		FOpenMobileAdsError Error,
		FName Source
	)
	{
		if (!Error.IsSet() && Error.NativeDiagnostics.IsSet())
		{
			FOpenMobileAdsErrorMappingContext Context;
			Context.Domain = EOpenMobileAdsErrorDomain::Consent;
			Context.Stage = EOpenMobileAdsFailureStage::Consent;
			Context.Provider = Source;
			Context.NativeCode = Error.NativeDiagnostics.NativeCode;
			Context.NativeMessage = Error.NativeDiagnostics.NativeMessage;
			Error = FOpenMobileAdsErrorMapper::FromNative(Context);
		}
		else if (!Error.IsSet())
		{
			Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The consent provider operation failed."),
				Source,
				TEXT("Check the consent provider diagnostics and retry when allowed.")
			);
		}
		Error.Stage = EOpenMobileAdsFailureStage::Consent;
		Error.Provider = Source;
		if (Error.Explanation.IsEmpty())
		{
			Error.Explanation = TEXT("The consent provider operation failed.");
		}
		if (Error.LikelyCause.IsEmpty())
		{
			Error.LikelyCause = Error.Explanation;
		}
		if (Error.SuggestedCorrection.IsEmpty())
		{
			Error.SuggestedCorrection =
				TEXT("Check the consent provider diagnostics and retry when allowed.");
		}
		if (Error.NativeDiagnostics.IsSet())
		{
			Error.NativeDiagnostics.Provider = Source;
		}
		return Error;
	}

	class FContextualEventSink final : public IOpenMobileAdsProviderEventSink
	{
	public:
		FContextualEventSink(
			TSharedRef<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe> InDispatcher,
			FName InProvider,
			FName InPlacement,
			EOpenMobileAdFormat InFormat,
			EOpenMobileAdsFailureStage InOperationStage,
			FGuid InRequestId,
			FGuid InCachedAdId = FGuid(),
			FString InFallbackRewardType = FString(),
			int64 InFallbackRewardAmount = 0
		)
			: Dispatcher(MoveTemp(InDispatcher))
			, Provider(InProvider)
			, Placement(InPlacement)
			, Format(InFormat)
			, OperationStage(InOperationStage)
			, RequestId(InRequestId)
			, CachedAdId(InCachedAdId)
			, FallbackRewardType(MoveTemp(InFallbackRewardType))
			, FallbackRewardAmount(InFallbackRewardAmount)
		{
		}

		virtual void Submit(FOpenMobileAdsEvent Event) override
		{
			bool bForward = false;
			{
				FScopeLock Lock(&Mutex);
				if (!bValid || !TryAcceptEvent(Event.Type))
				{
					return;
				}
				Normalize(Event);
				if (!bCommitted)
				{
					PendingEvents.Add(MoveTemp(Event));
					return;
				}
				bForward = true;
			}
			if (bForward)
			{
				Dispatcher->Submit(MoveTemp(Event));
			}
		}

		void Commit()
		{
			FScopeLock Lock(&Mutex);
			if (!bValid || bCommitted)
			{
				return;
			}
			for (FOpenMobileAdsEvent& Event : PendingEvents)
			{
				Dispatcher->Submit(MoveTemp(Event));
			}
			PendingEvents.Reset();
			bCommitted = true;
		}

		virtual void Invalidate() override
		{
			FScopeLock Lock(&Mutex);
			bValid = false;
			PendingEvents.Reset();
		}

	private:
		bool TryAcceptEvent(EOpenMobileAdsEventType Type)
		{
			if (OperationStage == EOpenMobileAdsFailureStage::Load)
			{
				if (
					bTerminalSubmitted
					|| (Type != EOpenMobileAdsEventType::Loaded
						&& Type != EOpenMobileAdsEventType::LoadFailed)
				)
				{
					return false;
				}
				bTerminalSubmitted = true;
				return true;
			}
			if (OperationStage == EOpenMobileAdsFailureStage::Hide)
			{
				if (
					bTerminalSubmitted
					|| (Type != EOpenMobileAdsEventType::Hidden
						&& Type != EOpenMobileAdsEventType::Failed)
				)
				{
					return false;
				}
				bTerminalSubmitted = true;
				return true;
			}
			if (OperationStage != EOpenMobileAdsFailureStage::Show)
			{
				return true;
			}
			if (Type == EOpenMobileAdsEventType::RewardEarned)
			{
				if (bRewardSubmitted || (bTerminalSubmitted && !bDismissedSubmitted))
				{
					return false;
				}
				bRewardSubmitted = true;
				return true;
			}
			if (bTerminalSubmitted)
			{
				return false;
			}
			switch (Type)
			{
			case EOpenMobileAdsEventType::Shown:
				if (bShownSubmitted)
				{
					return false;
				}
				bShownSubmitted = true;
				return true;

			case EOpenMobileAdsEventType::Impression:
			case EOpenMobileAdsEventType::Clicked:
			case EOpenMobileAdsEventType::RevenuePaid:
			case EOpenMobileAdsEventType::Refreshed:
				return true;

			case EOpenMobileAdsEventType::Dismissed:
				bDismissedSubmitted = true;
				bTerminalSubmitted = true;
				return true;

			case EOpenMobileAdsEventType::Failed:
				bTerminalSubmitted = true;
				return true;

			default:
				return false;
			}
		}

		void Normalize(FOpenMobileAdsEvent& Event) const
		{
			Event.Provider = Provider;
			Event.Placement = Placement;
			Event.Format = Format;
			Event.RequestId = RequestId;
			if (CachedAdId.IsValid())
			{
				Event.CachedAdId = CachedAdId;
			}
			if (Event.Type == EOpenMobileAdsEventType::RewardEarned)
			{
				if (Event.Reward.Type.IsEmpty() && !FallbackRewardType.IsEmpty())
				{
					Event.Reward.Type = FallbackRewardType;
				}
				if (Event.Reward.Amount == 0 && FallbackRewardAmount > 0)
				{
					Event.Reward.Amount = FallbackRewardAmount;
				}
				if (Event.Reward.Amount <= 0)
				{
					Event.Reward.Amount = 0;
					Event.bHasReward = false;
				}
			}
			if (
				Event.Type == EOpenMobileAdsEventType::LoadFailed
				|| Event.Type == EOpenMobileAdsEventType::Failed
			)
			{
				const TCHAR* FallbackExplanation =
					Event.Type == EOpenMobileAdsEventType::LoadFailed
						? TEXT("The ads provider failed the load request without a typed error.")
						: OperationStage == EOpenMobileAdsFailureStage::Show
						? TEXT("The ads provider failed the show request without a typed error.")
						: OperationStage == EOpenMobileAdsFailureStage::Teardown
						? TEXT("The ads provider failed the destroy request without a typed error.")
						: OperationStage == EOpenMobileAdsFailureStage::Hide
						? TEXT("The ads provider failed the hide request without a typed error.")
						: TEXT("The ads provider failed the operation without a typed error.");
				Event.Error = NormalizeProviderError(
					MoveTemp(Event.Error),
					OperationStage,
					Placement,
					Provider,
					FallbackExplanation
				);
				if (
					Event.Error.NativeDiagnostics.Network.IsEmpty()
					&& !Event.Network.IsEmpty()
				)
				{
					Event.Error.NativeDiagnostics.Provider = Provider;
					Event.Error.NativeDiagnostics.Network = Event.Network;
				}
				if (Event.Network.IsEmpty())
				{
					Event.Network = Event.Error.NativeDiagnostics.Network;
				}
			}
		}

		FCriticalSection Mutex;
		TSharedRef<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe> Dispatcher;
		TArray<FOpenMobileAdsEvent> PendingEvents;
		FName Provider;
		FName Placement;
		EOpenMobileAdFormat Format;
		EOpenMobileAdsFailureStage OperationStage;
		FGuid RequestId;
		FGuid CachedAdId;
		FString FallbackRewardType;
		int64 FallbackRewardAmount = 0;
		bool bCommitted = false;
		bool bShownSubmitted = false;
		bool bRewardSubmitted = false;
		bool bDismissedSubmitted = false;
		bool bTerminalSubmitted = false;
		bool bValid = true;
	};

	FOpenMobileAdsError MakeOperationThreadError(
		FName Placement,
		EOpenMobileAdsFailureStage Stage
	)
	{
		return FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			Stage,
			Placement,
			TEXT("Ads operations must begin on the Unreal game thread."),
			NAME_None,
			TEXT("Call the ads subsystem from gameplay or dispatch the call to the game thread.")
		);
	}

	FOpenMobileAdsError MakeUnsupportedFormatError(
		FName Placement,
		FName Provider,
		EOpenMobileAdsFailureStage Stage
	)
	{
		return FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::UnsupportedFormat,
			Stage,
			Placement,
			TEXT("The selected ads provider does not support this operation for the placement format."),
			Provider,
			TEXT("Choose a supported format or select another provider.")
		);
	}

	FOpenMobileAdsError MakeServiceNotReadyError(
		FName Placement,
		EOpenMobileAdsServiceState State,
		const FOpenMobileAdsError& InitializationError
	)
	{
		if (State == EOpenMobileAdsServiceState::Failed && InitializationError.IsSet())
		{
			FOpenMobileAdsError Error = InitializationError;
			Error.Placement = Placement;
			return Error;
		}
		return FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Initialization,
			Placement,
			State == EOpenMobileAdsServiceState::Initializing
				? TEXT("The ads service is still initializing.")
				: TEXT("The ads service has not been initialized."),
			NAME_None,
			TEXT("Call Initialize Ads and wait for the service to become ready.")
		);
	}

	FOpenMobileAdsError MakeOfflineError(
		FName Placement,
		FName Provider,
		EOpenMobileAdsFailureStage Stage
	)
	{
		return FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Offline,
			Stage,
			Placement,
			TEXT("The platform reports no active network connection."),
			Provider,
			TEXT("Retry after the platform reports a possible network connection."),
			true,
			TEXT("The device is offline or airplane mode is enabled.")
		);
	}

	FOpenMobileAdsError MakeShowPolicyError(
		FName Placement,
		FName Provider,
		const FOpenMobileAdsCanShowResult& Decision
	)
	{
		EOpenMobileAdsErrorCode Code = EOpenMobileAdsErrorCode::InvalidState;
		switch (Decision.BlockReason)
		{
		case EOpenMobileAdsCanShowBlockReason::UnknownPlacement:
			Code = EOpenMobileAdsErrorCode::UnknownPlacement;
			break;
		case EOpenMobileAdsCanShowBlockReason::Disabled:
			Code = EOpenMobileAdsErrorCode::DisabledPlacement;
			break;
		case EOpenMobileAdsCanShowBlockReason::ProviderUnavailable:
			Code = EOpenMobileAdsErrorCode::ProviderUnavailable;
			break;
		case EOpenMobileAdsCanShowBlockReason::UnsupportedFormat:
			Code = EOpenMobileAdsErrorCode::UnsupportedFormat;
			break;
		case EOpenMobileAdsCanShowBlockReason::PrivacyBlocked:
			Code = EOpenMobileAdsErrorCode::PrivacyBlocked;
			break;
		case EOpenMobileAdsCanShowBlockReason::Loading:
			Code = EOpenMobileAdsErrorCode::Busy;
			break;
		case EOpenMobileAdsCanShowBlockReason::NotLoaded:
		case EOpenMobileAdsCanShowBlockReason::Expired:
			Code = EOpenMobileAdsErrorCode::NotReady;
			break;
		case EOpenMobileAdsCanShowBlockReason::FrequencyCap:
			Code = EOpenMobileAdsErrorCode::FrequencyCap;
			break;
		case EOpenMobileAdsCanShowBlockReason::Offline:
			Code = EOpenMobileAdsErrorCode::Offline;
			break;
		case EOpenMobileAdsCanShowBlockReason::NotInitialized:
		case EOpenMobileAdsCanShowBlockReason::Cooldown:
		case EOpenMobileAdsCanShowBlockReason::LifecycleConflict:
		case EOpenMobileAdsCanShowBlockReason::None:
		default:
			break;
		}

		return FOpenMobileAdsError::Make(
			Code,
			EOpenMobileAdsFailureStage::Show,
			Placement,
			Decision.Explanation,
			Provider,
			TEXT("Resolve the reported blocker before showing the placement."),
			Code == EOpenMobileAdsErrorCode::Busy
				|| Code == EOpenMobileAdsErrorCode::NotReady
				|| Code == EOpenMobileAdsErrorCode::Offline
				|| (
					Code == EOpenMobileAdsErrorCode::FrequencyCap
					&& Decision.FrequencyCapScope
						== EOpenMobileAdsFrequencyCapScope::RollingWindow
				)
				|| Code == EOpenMobileAdsErrorCode::InvalidState
		);
	}

	FOpenMobileAdsError NormalizeInitializationError(
		FOpenMobileAdsError Error,
		FName ProviderName,
		const TCHAR* FallbackExplanation
	)
	{
		return NormalizeProviderError(
			MoveTemp(Error),
			EOpenMobileAdsFailureStage::Initialization,
			NAME_None,
			ProviderName,
			FallbackExplanation
		);
	}
}

struct FOpenMobileAdsActiveRequestContext
{
	FOpenMobileAdsLoadRequest LoadRequest;
	FName Placement;
	FName Provider;
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;
	EOpenMobileAdsFailureStage Stage = EOpenMobileAdsFailureStage::Internal;
	TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink;
	TMap<FName, FOpenMobileAdsPlacementStatus> PreviousStatuses;
	FOpenMobileAdsRetryScheduleHandle RetryScheduleHandle;
	double PendingRetryDelaySeconds = 0.0;
	int32 RetryAttempts = 0;
	int32 PlacementMaxRetryAttempts = -1;
	bool bRestoreStatusesOnFailure = true;
	bool bRetryPending = false;
	bool bWaitingForConnectivity = false;
	bool bProviderAttemptActive = true;
	bool bPreserveCachedAdOnHide = false;
};

struct FOpenMobileAdsAutomaticPreloadContext
{
	FOpenMobileAdsRetryScheduleHandle ScheduleHandle;
	double EarliestStartMonotonicSeconds = 0.0;
};

namespace OpenMobileAdsPrivate
{
	bool UsesFullscreenLifecycle(EOpenMobileAdFormat Format)
	{
		switch (Format)
		{
		case EOpenMobileAdFormat::Interstitial:
		case EOpenMobileAdFormat::Rewarded:
		case EOpenMobileAdFormat::RewardedInterstitial:
		case EOpenMobileAdFormat::AppOpen:
			return true;
		default:
			return false;
		}
	}

	bool HasReusableCachedAdState(EOpenMobileAdPlacementState State)
	{
		return State == EOpenMobileAdPlacementState::Ready
			|| State == EOpenMobileAdPlacementState::Hidden;
	}

	EOpenMobileErrorCode ToLegacyErrorCode(EOpenMobileAdsErrorCode Code)
	{
		switch (Code)
		{
		case EOpenMobileAdsErrorCode::NotConfigured:
			return EOpenMobileErrorCode::NotConfigured;
		case EOpenMobileAdsErrorCode::ProviderUnavailable:
		case EOpenMobileAdsErrorCode::NotReady:
		case EOpenMobileAdsErrorCode::Offline:
		case EOpenMobileAdsErrorCode::NoFill:
		case EOpenMobileAdsErrorCode::PrivacyBlocked:
			return EOpenMobileErrorCode::Unavailable;
		case EOpenMobileAdsErrorCode::UnsupportedPlatform:
		case EOpenMobileAdsErrorCode::UnsupportedFormat:
			return EOpenMobileErrorCode::NotSupported;
		case EOpenMobileAdsErrorCode::UnknownPlacement:
		case EOpenMobileAdsErrorCode::InvalidPlacement:
		case EOpenMobileAdsErrorCode::DisabledPlacement:
			return EOpenMobileErrorCode::InvalidArgument;
		case EOpenMobileAdsErrorCode::Busy:
			return EOpenMobileErrorCode::Busy;
		case EOpenMobileAdsErrorCode::Cancelled:
			return EOpenMobileErrorCode::Cancelled;
		case EOpenMobileAdsErrorCode::ProviderFailure:
		case EOpenMobileAdsErrorCode::NativeFailure:
			return EOpenMobileErrorCode::NativeFailure;
		default:
			return EOpenMobileErrorCode::Internal;
		}
	}

	FOpenMobileError ToLegacyError(const FOpenMobileAdsError& Error)
	{
		return FOpenMobileError::Make(
			ToLegacyErrorCode(Error.Code),
			Error.Explanation,
			Error.NativeDiagnostics.NativeCode,
			Error.Provider.ToString()
		);
	}

	IOpenMobileAdsProvider* FindRegisteredProvider(FName ProviderName)
	{
		const TArray<IOpenMobileAdsProvider*> Providers =
			IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileAdsProvider>(
				IOpenMobileAdsProvider::GetModularFeatureName()
			);
		for (IOpenMobileAdsProvider* Provider : Providers)
		{
			if (Provider && Provider->GetProviderName() == ProviderName)
			{
				return Provider;
			}
		}
		return nullptr;
	}

	void LogEvent(const FOpenMobileAdsEvent& Event)
	{
		EOpenMobileAdsLogLevel Level = EOpenMobileAdsLogLevel::Info;
		switch (Event.Type)
		{
		case EOpenMobileAdsEventType::LoadFailed:
		case EOpenMobileAdsEventType::Failed:
			Level = (
				Event.Error.Code == EOpenMobileAdsErrorCode::Cancelled
				|| Event.Error.Code == EOpenMobileAdsErrorCode::NoFill
			)
				? EOpenMobileAdsLogLevel::Info
				: Event.Error.bRetryable
				? EOpenMobileAdsLogLevel::Warning
				: EOpenMobileAdsLogLevel::Error;
			break;
		case EOpenMobileAdsEventType::ProviderRegistered:
		case EOpenMobileAdsEventType::ProviderUnregistered:
		case EOpenMobileAdsEventType::PlacementStateChanged:
		case EOpenMobileAdsEventType::LoadStarted:
		case EOpenMobileAdsEventType::ShowAccepted:
		case EOpenMobileAdsEventType::Refreshed:
		case EOpenMobileAdsEventType::Destroyed:
		case EOpenMobileAdsEventType::Expired:
		case EOpenMobileAdsEventType::Hidden:
			Level = EOpenMobileAdsLogLevel::Verbose;
			break;
		case EOpenMobileAdsEventType::Impression:
		case EOpenMobileAdsEventType::Clicked:
		case EOpenMobileAdsEventType::RewardEarned:
		case EOpenMobileAdsEventType::RevenuePaid:
			Level = EOpenMobileAdsLogLevel::VeryVerbose;
			break;
		default:
			break;
		}

		FString Message = StaticEnum<EOpenMobileAdsEventType>()->GetNameStringByValue(
			static_cast<int64>(Event.Type)
		);
		if (Event.Error.IsSet())
		{
			Message += FString::Printf(TEXT(": %s"), *Event.Error.Explanation);
		}
		FOpenMobileAdsLog::Write(
			Level,
			Message,
			Event.Placement,
			Event.Provider
		);
	}
}

void UOpenMobileAdsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bDeinitialized = false;
	EnsureRuntime();
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::RefreshConsent()
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				NAME_None,
				EOpenMobileAdsFailureStage::Consent
			)
		);
	}

	EnsureRuntime();
	if (bDeinitialized)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Consent,
			NAME_None,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}
	if (ActiveConsentRequestId.IsValid())
	{
		if (bPrivacyOptionsPresentationActive)
		{
			return FOpenMobileAdsOperationResult::Rejected(
				FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::Busy,
					EOpenMobileAdsFailureStage::Consent,
					NAME_None,
					TEXT("Consent cannot refresh while privacy options are open."),
					ActiveConsentProviderName,
					TEXT("Retry after the privacy-options form closes."),
					true
				)
			);
		}
		return FOpenMobileAdsOperationResult::Accepted(ActiveConsentRequestId);
	}
	if (
		!bApplicationActive
		|| !bApplicationInForeground
		|| (FullscreenLifecycle && FullscreenLifecycle->IsOccupied())
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Busy,
			EOpenMobileAdsFailureStage::Consent,
			NAME_None,
			TEXT("Consent cannot refresh while another full-screen surface is active."),
			NAME_None,
			TEXT("Retry after the application is active and the current full-screen surface closes."),
			true
		));
	}

	FOpenMobileAdsError SelectionError;
	IOpenMobileAdsProvider* Provider = FindProvider(&SelectionError);
	if (!Provider)
	{
		SelectionError.Stage = EOpenMobileAdsFailureStage::Consent;
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(SelectionError));
	}
	const FName ConsentProviderName = Provider->GetConsentProviderName();
	if (ConsentProviderName.IsNone())
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderUnavailable,
			EOpenMobileAdsFailureStage::Consent,
			NAME_None,
			TEXT("The selected ads provider does not supply a consent provider."),
			Provider->GetProviderName(),
			TEXT("Enable a provider plugin with a supported consent implementation.")
		));
	}

	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	ActiveConsentRequest = FOpenMobileAdsConsentRequest();
	ActiveConsentRequest.RequestId = FGuid::NewGuid();
	ActiveConsentRequest.Platform = OpenMobileAdsGetCurrentPlatform();
	ActiveConsentRequest.Development =
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			Settings->IsDevelopmentTestModeEnabled(),
			Settings->TestDeviceIdentifiers,
			Settings->DebugGeography
		);
	ActiveConsentRequest.Privacy = Settings->Privacy;
	ActiveConsentRequest.Privacy.ChildDirectedTreatment =
		PrivacySnapshot.ChildDirectedTreatment;
	ActiveConsentRequest.Privacy.UnderAgeOfConsent =
		PrivacySnapshot.UnderAgeOfConsent;
	if (
		ActiveConsentRequest.Privacy.UnderAgeOfConsent
			== EOpenMobileAdsAgeTreatment::Yes
	)
	{
		ActiveConsentRequest.Development.bEnableConsentDebug = false;
	}
	ActiveConsentRequestId = ActiveConsentRequest.RequestId;
	ActiveConsentAdsProviderName = Provider->GetProviderName();
	ActiveConsentProviderName = ConsentProviderName;
	ApplyConsentStatusUpdateOnGameThread(
		FOpenMobileAdsConsentStatusUpdate::BeginRefresh(ConsentProviderName)
	);

	const FGuid RequestId = ActiveConsentRequestId;
	const FName AdsProviderName = ActiveConsentAdsProviderName;
	const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
	const TSharedRef<OpenMobileAdsPrivate::FConsentProviderSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FConsentProviderSink, ESPMode::ThreadSafe>(
			[WeakThis, RequestId, AdsProviderName, ConsentProviderName](
				FOpenMobileAdsConsentStatusUpdate Update
			) mutable
			{
				AsyncTask(
					ENamedThreads::GameThread,
					[WeakThis, RequestId, AdsProviderName, ConsentProviderName,
						Update = MoveTemp(Update)]() mutable
					{
						if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
						{
							Subsystem->HandleConsentRefreshCompleted(
								RequestId,
								AdsProviderName,
								ConsentProviderName,
								MoveTemp(Update)
							);
						}
					}
				);
			},
			[WeakThis, RequestId, AdsProviderName, ConsentProviderName](
				FOpenMobileAdsError Error
			) mutable
			{
				AsyncTask(
					ENamedThreads::GameThread,
					[WeakThis, RequestId, AdsProviderName, ConsentProviderName,
						Error = MoveTemp(Error)]() mutable
					{
						if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
						{
							Subsystem->HandleConsentOperationFailed(
								RequestId,
								AdsProviderName,
								ConsentProviderName,
								MoveTemp(Error)
							);
						}
					}
				);
			}
		);
	ConsentOperationSink = Sink;

	FOpenMobileAdsError ProviderError;
	if (!Provider->RefreshConsent(ActiveConsentRequest, Sink, ProviderError))
	{
		Sink->Invalidate();
		ConsentOperationSink.Reset();
		FOpenMobileAdsError Error = OpenMobileAdsPrivate::NormalizeConsentError(
			MoveTemp(ProviderError),
			ConsentProviderName
		);
		ClearConsentOperation(false);
		ApplyConsentStatusUpdateOnGameThread(
			FOpenMobileAdsConsentStatusUpdate::Fail(
				ConsentProviderName,
				Error
			)
		);
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}
	Sink->Commit();
	return FOpenMobileAdsOperationResult::Accepted(RequestId);
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::ResetConsentForTesting()
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				NAME_None,
				EOpenMobileAdsFailureStage::Consent
			)
		);
	}

#if UE_BUILD_SHIPPING
	return FOpenMobileAdsOperationResult::Rejected(
		FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Consent,
			NAME_None,
			TEXT("Consent reset is disabled in Shipping builds."),
			NAME_None,
			TEXT("Use a non-shipping build with Development/Test Mode enabled.")
		)
	);
#else
	EnsureRuntime();
	if (bDeinitialized)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Cancelled,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The ads subsystem has been deinitialized.")
			)
		);
	}

	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	if (!Settings->IsDevelopmentTestModeEnabled())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::InvalidState,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("Consent reset requires Development/Test Mode."),
				NAME_None,
				TEXT("Enable Development/Test Mode in a non-shipping build.")
			)
		);
	}
	if (
		ActiveConsentRequestId.IsValid()
		|| (FullscreenLifecycle && FullscreenLifecycle->IsOccupied())
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Busy,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("Consent cannot reset during another consent or full-screen operation."),
				ActiveConsentProviderName,
				TEXT("Retry after the active consent or ad operation finishes."),
				true
			)
		);
	}

	FOpenMobileAdsError SelectionError;
	IOpenMobileAdsProvider* Provider = FindProvider(&SelectionError);
	if (!Provider)
	{
		SelectionError.Stage = EOpenMobileAdsFailureStage::Consent;
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(SelectionError));
	}
	const FName AdsProviderName = Provider->GetProviderName();
	const FName ConsentProviderName = Provider->GetConsentProviderName();
	if (ConsentProviderName.IsNone())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The selected ads provider does not supply a consent provider."),
				AdsProviderName,
				TEXT("Enable a provider plugin with a supported consent implementation.")
			)
		);
	}

	const FGuid RequestId = FGuid::NewGuid();
	ActiveConsentRequestId = RequestId;
	ActiveConsentAdsProviderName = AdsProviderName;
	ActiveConsentProviderName = ConsentProviderName;
	ApplyConsentStatusUpdateOnGameThread(
		FOpenMobileAdsConsentStatusUpdate::BeginReset(ConsentProviderName)
	);
	if (
		bDeinitialized
		|| ActiveConsentRequestId != RequestId
		|| ActiveConsentAdsProviderName != AdsProviderName
		|| ActiveConsentProviderName != ConsentProviderName
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Cancelled,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("Consent reset was cancelled before reaching the provider."),
				ConsentProviderName
			)
		);
	}

	Provider = OpenMobileAdsPrivate::FindRegisteredProvider(AdsProviderName);
	FOpenMobileAdsError ProviderError;
	if (
		!Provider
		|| !Provider->IsSupported()
		|| Provider->GetConsentProviderName() != ConsentProviderName
		|| !Provider->SupportsConsentResetForTesting()
		|| !Provider->ResetConsentForTesting(ProviderError)
	)
	{
		if (!ProviderError.IsSet())
		{
			ProviderError = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The selected consent provider cannot reset persistent state."),
				ConsentProviderName,
				TEXT("Use a provider with a development consent reset implementation.")
			);
		}
		FOpenMobileAdsError Error = OpenMobileAdsPrivate::NormalizeConsentError(
			MoveTemp(ProviderError),
			ConsentProviderName
		);
		ClearConsentOperation(false);
		ApplyConsentStatusUpdateOnGameThread(
			FOpenMobileAdsConsentStatusUpdate::Fail(
				ConsentProviderName,
				Error
			)
		);
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}

	ClearConsentOperation(false);
	ApplyConsentStatusUpdateOnGameThread(
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::Unknown,
			EOpenMobileAdsGdprApplicability::Unknown,
			EOpenMobileAdsConsentRequirement::Unknown,
			EOpenMobileAdsConsentRequestState::Unknown,
			ConsentProviderName,
			{},
			false
		)
	);
	return FOpenMobileAdsOperationResult::Accepted(RequestId);
#endif
}

FOpenMobileAdsOperationResult
UOpenMobileAdsSubsystem::PresentPrivacyOptionsForm()
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				NAME_None,
				EOpenMobileAdsFailureStage::Consent
			)
		);
	}

	EnsureRuntime();
	if (bDeinitialized)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Cancelled,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The ads subsystem has been deinitialized.")
			)
		);
	}
	if (ActiveConsentRequestId.IsValid())
	{
		if (bPrivacyOptionsPresentationActive)
		{
			return FOpenMobileAdsOperationResult::Accepted(
				ActiveConsentRequestId
			);
		}
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Busy,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("Privacy options cannot open during another consent operation."),
				ActiveConsentProviderName,
				TEXT("Retry after the active consent operation finishes."),
				true
			)
		);
	}
	if (
		!PrivacySnapshot.IsConsentStatusFreshAt(FDateTime::UtcNow())
		|| !PrivacySnapshot.UsPrivacy.bPrivacyOptionsFormAvailable
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("A current privacy-options form is not available."),
				PrivacySnapshot.Source,
				TEXT("Refresh consent information before opening privacy options.")
			)
		);
	}
	if (
		!bApplicationActive
		|| !bApplicationInForeground
		|| (FullscreenLifecycle && FullscreenLifecycle->IsOccupied())
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Busy,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("Privacy options cannot open while another full-screen surface is active."),
				PrivacySnapshot.Source,
				TEXT("Retry after the application is active and the current full-screen surface closes."),
				true
			)
		);
	}

	FOpenMobileAdsError SelectionError;
	IOpenMobileAdsProvider* Provider = FindProvider(&SelectionError);
	if (!Provider)
	{
		SelectionError.Stage = EOpenMobileAdsFailureStage::Consent;
		return FOpenMobileAdsOperationResult::Rejected(
			MoveTemp(SelectionError)
		);
	}
	const FName ConsentProviderName = Provider->GetConsentProviderName();
	if (
		ConsentProviderName.IsNone()
		|| !Provider->SupportsPrivacyOptionsForm()
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The selected consent provider does not support privacy options."),
				Provider->GetProviderName(),
				TEXT("Enable a consent provider with a privacy-options form.")
			)
		);
	}

	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	ActiveConsentRequest = FOpenMobileAdsConsentRequest();
	ActiveConsentRequest.RequestId = FGuid::NewGuid();
	ActiveConsentRequest.Platform = OpenMobileAdsGetCurrentPlatform();
	ActiveConsentRequest.Development =
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			Settings->IsDevelopmentTestModeEnabled(),
			Settings->TestDeviceIdentifiers,
			Settings->DebugGeography
		);
	ActiveConsentRequest.Privacy = Settings->Privacy;
	ActiveConsentRequest.Privacy.ChildDirectedTreatment =
		PrivacySnapshot.ChildDirectedTreatment;
	ActiveConsentRequest.Privacy.UnderAgeOfConsent =
		PrivacySnapshot.UnderAgeOfConsent;
	if (
		ActiveConsentRequest.Privacy.UnderAgeOfConsent
			== EOpenMobileAdsAgeTreatment::Yes
	)
	{
		ActiveConsentRequest.Development.bEnableConsentDebug = false;
	}
	ActiveConsentRequestId = ActiveConsentRequest.RequestId;
	ActiveConsentAdsProviderName = Provider->GetProviderName();
	ActiveConsentProviderName = ConsentProviderName;
	bPrivacyOptionsPresentationActive = true;
	const FGuid RequestId = ActiveConsentRequestId;
	if (!StartConsentForm(*Provider, true))
	{
		return FOpenMobileAdsOperationResult::Rejected(
			PrivacySnapshot.Error
		);
	}
	return FOpenMobileAdsOperationResult::Accepted(RequestId);
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::InitializeAds()
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				NAME_None,
				EOpenMobileAdsFailureStage::Initialization
			)
		);
	}

	EnsureRuntime();
	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	const bool bDevelopmentTestMode = Settings->IsDevelopmentTestModeEnabled();
	FOpenMobileAdsLog::SetDevelopmentTestMode(bDevelopmentTestMode);
	FOpenMobileAdsLog::SetTestDeviceIdentifiers(Settings->TestDeviceIdentifiers);
	if (bDeinitialized || ServiceState == EOpenMobileAdsServiceState::ShuttingDown)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Initialization,
			NAME_None,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}
	if (
		ServiceState == EOpenMobileAdsServiceState::Initializing
		|| ServiceState == EOpenMobileAdsServiceState::Ready
	)
	{
		return FOpenMobileAdsOperationResult::Accepted(InitializationRequestId);
	}
	if (ServiceState == EOpenMobileAdsServiceState::Failed)
	{
		return FOpenMobileAdsOperationResult::Rejected(InitializationError);
	}
	if (
		Settings->bEnableTrackingAuthorization
		&& Settings->bDelayAdsInitializationUntilTrackingAuthorization
		&& TrackingAuthorizationStatus
			== EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::PrivacyBlocked,
			EOpenMobileAdsFailureStage::Initialization,
			NAME_None,
			TEXT("Ads initialization is waiting for a tracking authorization decision."),
			NAME_None,
			TEXT("Request tracking authorization at the appropriate user journey point, then retry ads initialization."),
			true
		));
	}

	FOpenMobileAdsError SelectionError;
	IOpenMobileAdsProvider* Provider = FindProvider(&SelectionError);
	if (!Provider)
	{
		ServiceState = EOpenMobileAdsServiceState::Failed;
		InitializationError = MoveTemp(SelectionError);
		const FDateTime Now = FDateTime::UtcNow();
		InitializationStatus = FOpenMobileAdsInitializationStatusSnapshot();
		InitializationStatus.ServiceState = ServiceState;
		InitializationStatus.StartedAt = Now;
		InitializationStatus.LastUpdated = Now;
		InitializationStatus.LatencyMilliseconds = 0.0;
		InitializationStatus.Error = InitializationError;
		BroadcastInitializationStatus();
		return FOpenMobileAdsOperationResult::Rejected(InitializationError);
	}

	InitializationRequestId = FGuid::NewGuid();
	SelectedProviderName = Provider->GetProviderName();
	InitializationError = FOpenMobileAdsError();
	ServiceState = EOpenMobileAdsServiceState::Initializing;
	InitializationStartedSeconds = FPlatformTime::Seconds();
	InitializationStatus = FOpenMobileAdsInitializationStatusSnapshot();
	InitializationStatus.RequestId = InitializationRequestId;
	InitializationStatus.ServiceState = ServiceState;
	InitializationStatus.StartedAt = FDateTime::UtcNow();
	InitializationStatus.LastUpdated = InitializationStatus.StartedAt;
	FOpenMobileAdsInitializationComponentStatus ProviderStatus;
	ProviderStatus.Type = EOpenMobileAdsInitializationComponentType::Provider;
	ProviderStatus.Name = SelectedProviderName;
	ProviderStatus.State = EOpenMobileAdsInitializationState::Initializing;
	ProviderStatus.Capabilities = Provider->GetCapabilities();
	ProviderStatus.Version = ProviderStatus.Capabilities.ProviderVersion;
	ProviderStatus.bHasCapabilities = true;
	InitializationStatus.Components.Add(MoveTemp(ProviderStatus));
	BroadcastInitializationStatus();

	FOpenMobileAdsInitializationRequest Request;
	Request.RequestId = InitializationRequestId;
	Request.Platform = OpenMobileAdsGetCurrentPlatform();
	Request.Development =
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			bDevelopmentTestMode,
			Settings->TestDeviceIdentifiers,
			Settings->DebugGeography
		);
	Request.Privacy = Settings->Privacy;
	Request.Privacy.ChildDirectedTreatment =
		PrivacySnapshot.ChildDirectedTreatment;
	Request.Privacy.UnderAgeOfConsent = PrivacySnapshot.UnderAgeOfConsent;
	Request.PrivacyContext =
		OpenMobileAdsPrivate::MakeProviderPrivacyContext(PrivacySnapshot);
	if (
		Request.Privacy.UnderAgeOfConsent
			== EOpenMobileAdsAgeTreatment::Yes
	)
	{
		Request.Development.bEnableConsentDebug = false;
	}
	Request.RequestConfiguration = Settings->RequestConfiguration;
	PropagateConsentSignals(*Provider, false);

	const FGuid RequestId = InitializationRequestId;
	const FName ProviderName = SelectedProviderName;
	const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
	const TSharedRef<OpenMobileAdsPrivate::FInitializationSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FInitializationSink, ESPMode::ThreadSafe>(
			[WeakThis, RequestId, ProviderName](
				FOpenMobileAdsInitializationComponentStatus Status
			) mutable
			{
				AsyncTask(
					ENamedThreads::GameThread,
					[WeakThis, RequestId, ProviderName, Status = MoveTemp(Status)]() mutable
					{
						if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
						{
							Subsystem->HandleProviderInitializationStatus(
								RequestId,
								ProviderName,
								MoveTemp(Status)
							);
						}
					}
				);
			},
			[WeakThis, RequestId, ProviderName](FOpenMobileAdsError Error) mutable
			{
				AsyncTask(
					ENamedThreads::GameThread,
					[WeakThis, RequestId, ProviderName, Error = MoveTemp(Error)]() mutable
					{
						if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
						{
							Subsystem->HandleInitializationCompleted(
								RequestId,
								ProviderName,
								MoveTemp(Error)
							);
						}
					}
				);
			}
		);
	InitializationSink = Sink;

	FOpenMobileAdsError ProviderError;
	if (!Provider->Initialize(Request, Sink, ProviderError))
	{
		Sink->Invalidate();
		InitializationSink.Reset();
		ServiceState = EOpenMobileAdsServiceState::Failed;
		InitializationError = OpenMobileAdsPrivate::NormalizeInitializationError(
			MoveTemp(ProviderError),
			ProviderName,
			TEXT("The ads provider rejected SDK initialization without an error.")
		);
		InitializationStatus.ServiceState = ServiceState;
		InitializationStatus.Error = InitializationError;
		InitializationStatus.LatencyMilliseconds =
			(FPlatformTime::Seconds() - InitializationStartedSeconds) * 1000.0;
		if (FOpenMobileAdsInitializationComponentStatus* Component =
			InitializationStatus.Components.FindByPredicate(
				[ProviderName](const FOpenMobileAdsInitializationComponentStatus& Candidate)
				{
					return Candidate.Type == EOpenMobileAdsInitializationComponentType::Provider
						&& Candidate.Name == ProviderName;
				}
			))
		{
			Component->State = EOpenMobileAdsInitializationState::Failed;
			Component->LatencyMilliseconds = InitializationStatus.LatencyMilliseconds;
			Component->Error = InitializationError;
		}
		BroadcastInitializationStatus();
		return FOpenMobileAdsOperationResult::Rejected(InitializationError);
	}

	bProviderInitializationStarted = true;
	bChildDirectedTreatmentLocked = true;
	bUnderAgeOfConsentLocked = true;
	Sink->Commit();
	return FOpenMobileAdsOperationResult::Accepted(InitializationRequestId);
}

void UOpenMobileAdsSubsystem::HandleInitializationCompleted(
	FGuid RequestId,
	FName ProviderName,
	FOpenMobileAdsError Error
)
{
	check(IsInGameThread());
	if (
		bDeinitialized
		|| ServiceState != EOpenMobileAdsServiceState::Initializing
		|| RequestId != InitializationRequestId
		|| ProviderName != SelectedProviderName
	)
	{
		return;
	}

	if (Error.IsSet() || Error.NativeDiagnostics.IsSet())
	{
		if (InitializationSink)
		{
			InitializationSink->Invalidate();
			InitializationSink.Reset();
		}
		InitializationError = OpenMobileAdsPrivate::NormalizeInitializationError(
			MoveTemp(Error),
			ProviderName,
			TEXT("The ads provider failed to initialize.")
		);
		ServiceState = EOpenMobileAdsServiceState::Failed;
		InitializationStatus.Error = InitializationError;
		if (FOpenMobileAdsInitializationComponentStatus* Component =
			InitializationStatus.Components.FindByPredicate(
				[ProviderName](const FOpenMobileAdsInitializationComponentStatus& Candidate)
				{
					return Candidate.Type == EOpenMobileAdsInitializationComponentType::Provider
						&& Candidate.Name == ProviderName;
				}
			))
		{
			Component->State = EOpenMobileAdsInitializationState::Failed;
			Component->LatencyMilliseconds =
				(FPlatformTime::Seconds() - InitializationStartedSeconds) * 1000.0;
			Component->Error = InitializationError;
		}
		InitializationStatus.ServiceState = ServiceState;
		InitializationStatus.LatencyMilliseconds =
			(FPlatformTime::Seconds() - InitializationStartedSeconds) * 1000.0;
		BroadcastInitializationStatus();
		return;
	}

	InitializationError = FOpenMobileAdsError();
	ServiceState = EOpenMobileAdsServiceState::Ready;
	InitializationStatus.Error = FOpenMobileAdsError();
	InitializationStatus.ServiceState = ServiceState;
	InitializationStatus.LatencyMilliseconds =
		(FPlatformTime::Seconds() - InitializationStartedSeconds) * 1000.0;
	if (FOpenMobileAdsInitializationComponentStatus* Component =
		InitializationStatus.Components.FindByPredicate(
			[ProviderName](const FOpenMobileAdsInitializationComponentStatus& Candidate)
			{
				return Candidate.Type == EOpenMobileAdsInitializationComponentType::Provider
					&& Candidate.Name == ProviderName;
			}
		))
	{
		Component->State = EOpenMobileAdsInitializationState::Ready;
		Component->LatencyMilliseconds = InitializationStatus.LatencyMilliseconds;
		Component->Error = FOpenMobileAdsError();
	}
	UpdatePartialInitializationState();
	BroadcastInitializationStatus();
	RequestConfiguredAutomaticPreloads();
}

void UOpenMobileAdsSubsystem::HandleProviderInitializationStatus(
	FGuid RequestId,
	FName ProviderName,
	FOpenMobileAdsInitializationComponentStatus Status
)
{
	check(IsInGameThread());
	if (
		bDeinitialized
		|| RequestId != InitializationRequestId
		|| ProviderName != SelectedProviderName
		|| (
			ServiceState != EOpenMobileAdsServiceState::Initializing
			&& ServiceState != EOpenMobileAdsServiceState::Ready
		)
	)
	{
		return;
	}
	if (Status.Type == EOpenMobileAdsInitializationComponentType::Provider)
	{
		Status.Name = ProviderName;
	}
	else if (Status.Parent.IsNone())
	{
		Status.Parent = ProviderName;
	}
	if (Status.Name.IsNone())
	{
		return;
	}
	if (
		Status.State == EOpenMobileAdsInitializationState::Failed
		|| Status.Error.IsSet()
		|| Status.Error.NativeDiagnostics.IsSet()
	)
	{
		if (Status.Error.NativeDiagnostics.Network.IsEmpty())
		{
			if (Status.Type == EOpenMobileAdsInitializationComponentType::Network)
			{
				Status.Error.NativeDiagnostics.Network = Status.Name.ToString();
			}
			else if (Status.Type == EOpenMobileAdsInitializationComponentType::Adapter)
			{
				Status.Error.NativeDiagnostics.Network = Status.Parent.ToString();
			}
		}
		Status.Error = OpenMobileAdsPrivate::NormalizeProviderError(
			MoveTemp(Status.Error),
			EOpenMobileAdsFailureStage::Initialization,
			NAME_None,
			ProviderName,
			TEXT("The ads provider reported an initialization component failure without a typed error.")
		);
	}
	UpsertInitializationComponent(MoveTemp(Status));
	UpdatePartialInitializationState();
	BroadcastInitializationStatus();
}

void UOpenMobileAdsSubsystem::UpsertInitializationComponent(
	FOpenMobileAdsInitializationComponentStatus Status
)
{
	FOpenMobileAdsInitializationComponentStatus* Existing =
		InitializationStatus.Components.FindByPredicate(
			[&Status](const FOpenMobileAdsInitializationComponentStatus& Candidate)
			{
				return Candidate.Type == Status.Type
					&& Candidate.Name == Status.Name
					&& Candidate.Parent == Status.Parent;
			}
		);
	if (!Existing)
	{
		InitializationStatus.Components.Add(MoveTemp(Status));
		return;
	}
	if (Status.Version.IsEmpty())
	{
		Status.Version = Existing->Version;
	}
	if (!Status.bHasCapabilities && Existing->bHasCapabilities)
	{
		Status.bHasCapabilities = true;
		Status.Capabilities = Existing->Capabilities;
	}
	*Existing = MoveTemp(Status);
}

void UOpenMobileAdsSubsystem::UpdatePartialInitializationState()
{
	InitializationStatus.bPartialSuccess =
		ServiceState == EOpenMobileAdsServiceState::Ready
		&& InitializationStatus.Components.ContainsByPredicate(
			[](const FOpenMobileAdsInitializationComponentStatus& Component)
			{
				return Component.Type != EOpenMobileAdsInitializationComponentType::Provider
					&& Component.State != EOpenMobileAdsInitializationState::Ready;
			}
		);
}

void UOpenMobileAdsSubsystem::BroadcastInitializationStatus()
{
	check(IsInGameThread());
	InitializationStatus.ServiceState = ServiceState;
	InitializationStatus.LastUpdated = FDateTime::UtcNow();
	RefreshCanRequestAdsDecision();
	NativeInitializationStatusChanged.Broadcast(InitializationStatus);
	OnInitializationStatusChanged.Broadcast(InitializationStatus);
}

void UOpenMobileAdsSubsystem::BroadcastConsentStatus()
{
	check(IsInGameThread());
	RefreshCanRequestAdsDecision();
	StopPrivacyBlockedRetries();
	ReevaluateAutomaticPreloads();
	NativeConsentStatusChanged.Broadcast(PrivacySnapshot);
	OnConsentStatusChanged.Broadcast(PrivacySnapshot);
}

void UOpenMobileAdsSubsystem::PropagateConsentSignals(
	IOpenMobileAdsProvider& Provider,
	bool bRuntimeUpdate
)
{
	check(IsInGameThread());
	const FOpenMobileAdsConsentSignals Signals =
		OpenMobileAdsPrivate::MakeConsentSignals(PrivacySnapshot);
	const int32 ChangedSignals = bConsentSignalsPropagated
		? Signals.GetChangedSignalMask(LastPropagatedConsentSignals)
		: Signals.GetConfiguredSignalMask();
	if (bRuntimeUpdate && ChangedSignals == 0)
	{
		return;
	}

	const int32 ConfiguredSignals = Signals.GetConfiguredSignalMask();
	const int32 RequiredSignals = Signals.GetRequiredSignalMask();
	const int32 OperationSignals = bRuntimeUpdate
		? ChangedSignals
		: ConfiguredSignals;
	const FOpenMobileAdsConsentSignalDeliverySnapshot PreviousStatus =
		ConsentSignalDeliveryStatus;
	FOpenMobileAdsConsentSignalDeliverySnapshot NextStatus;

	auto ApplyToConsumer = [
		&Signals,
		bRuntimeUpdate,
		ConfiguredSignals,
		RequiredSignals,
		ChangedSignals,
		OperationSignals,
		&PreviousStatus,
		&NextStatus
	](
		EOpenMobileAdsConsentSignalConsumerType Type,
		FName Name,
		FName Parent,
		int32 SupportedSignals,
		int32 ConfirmableSignals,
		int32 RuntimeSignals,
		auto&& Apply
	)
	{
		FOpenMobileAdsConsentSignalDeliveryStatus Status;
		Status.Type = Type;
		Status.Name = Name;
		Status.Parent = Parent;
		Status.ConfiguredSignals = ConfiguredSignals;
		Status.RequiredSignals = RequiredSignals;
		Status.bRuntimeUpdate = bRuntimeUpdate;
		if (const FOpenMobileAdsConsentSignalDeliveryStatus* Previous =
			PreviousStatus.Find(Type, Name, Parent))
		{
			Status.AppliedSignals = Previous->AppliedSignals;
			Status.ConfirmedSignals = Previous->ConfirmedSignals;
		}

		const int32 EligibleSignals = OperationSignals
			& SupportedSignals
			& (bRuntimeUpdate
				? RuntimeSignals
				: FOpenMobileAdsConsentSignals::AllSignalMask);
		FOpenMobileAdsConsentSignalApplyResult Result;
		if (!bRuntimeUpdate || EligibleSignals != 0)
		{
			Result = Apply(Signals, EligibleSignals);
		}
		if (bRuntimeUpdate)
		{
			Status.AppliedSignals &= ~ChangedSignals;
			Status.ConfirmedSignals &= ~ChangedSignals;
		}
		else
		{
			Status.AppliedSignals = 0;
			Status.ConfirmedSignals = 0;
		}
		const int32 AppliedSignals = Result.AppliedSignals
			& EligibleSignals
			& SupportedSignals;
		Status.AppliedSignals |= AppliedSignals;
		Status.ConfirmedSignals |= Result.ConfirmedSignals
			& AppliedSignals
			& ConfirmableSignals;
		Status.AppliedSignals &= ConfiguredSignals;
		Status.ConfirmedSignals &= Status.AppliedSignals;
		Status.Error = MoveTemp(Result.Error);

		const int32 MissingSupport = RequiredSignals & ~SupportedSignals;
		const int32 MissingRuntimeSupport = bRuntimeUpdate
			? RequiredSignals & ChangedSignals & ~RuntimeSignals
			: 0;
		const int32 MissingApplication =
			RequiredSignals & ~Status.AppliedSignals;
		const int32 MissingConfirmation =
			RequiredSignals & ~Status.ConfirmedSignals;
		if (Status.Error.IsSet() || Status.Error.NativeDiagnostics.IsSet())
		{
			Status.State = EOpenMobileAdsConsentSignalDeliveryState::Failed;
		}
		else if (
			MissingSupport != 0
			|| MissingRuntimeSupport != 0
			|| MissingApplication != 0
		)
		{
			Status.State = EOpenMobileAdsConsentSignalDeliveryState::Unsupported;
		}
		else if (MissingConfirmation != 0)
		{
			Status.State = EOpenMobileAdsConsentSignalDeliveryState::Unconfirmed;
		}
		else
		{
			Status.State = RequiredSignals == 0
				? EOpenMobileAdsConsentSignalDeliveryState::NotRequired
				: EOpenMobileAdsConsentSignalDeliveryState::Applied;
		}
		NextStatus.Consumers.Add(MoveTemp(Status));
	};

	ApplyToConsumer(
		EOpenMobileAdsConsentSignalConsumerType::Provider,
		Provider.GetProviderName(),
		NAME_None,
		Provider.GetSupportedConsentSignalMask(),
		Provider.GetConfirmableConsentSignalMask(),
		Provider.GetRuntimeUpdatableConsentSignalMask(),
		[&Provider](
			const FOpenMobileAdsConsentSignals& CurrentSignals,
			int32 SignalMask
		)
		{
			return Provider.ApplyConsentSignals(CurrentSignals, SignalMask);
		}
	);

	TArray<IOpenMobileAdsConsentSignalConsumer*> Consumers =
		IModularFeatures::Get().GetModularFeatureImplementations<
			IOpenMobileAdsConsentSignalConsumer
		>(IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName());
	Consumers.RemoveAll(
		[&Provider](const IOpenMobileAdsConsentSignalConsumer* Consumer)
		{
			if (!Consumer)
			{
				return true;
			}
			const EOpenMobileAdsConsentSignalConsumerType Type =
				Consumer->GetConsumerType();
			return Consumer->GetOwningProviderName()
					!= Provider.GetProviderName()
				|| Consumer->GetConsumerName().IsNone()
				|| (
					Type != EOpenMobileAdsConsentSignalConsumerType::Network
					&& Type != EOpenMobileAdsConsentSignalConsumerType::Adapter
				);
		}
	);
	Consumers.Sort(
		[](const IOpenMobileAdsConsentSignalConsumer& Left,
			const IOpenMobileAdsConsentSignalConsumer& Right)
		{
			if (Left.GetConsumerType() != Right.GetConsumerType())
			{
				return static_cast<uint8>(Left.GetConsumerType())
					< static_cast<uint8>(Right.GetConsumerType());
			}
			if (Left.GetParentName() != Right.GetParentName())
			{
				return Left.GetParentName().LexicalLess(Right.GetParentName());
			}
			return Left.GetConsumerName().LexicalLess(Right.GetConsumerName());
		}
	);
	for (IOpenMobileAdsConsentSignalConsumer* Consumer : Consumers)
	{
		ApplyToConsumer(
			Consumer->GetConsumerType(),
			Consumer->GetConsumerName(),
			Consumer->GetParentName(),
			Consumer->GetSupportedConsentSignalMask(),
			Consumer->GetConfirmableConsentSignalMask(),
			Consumer->GetRuntimeUpdatableConsentSignalMask(),
			[Consumer](
				const FOpenMobileAdsConsentSignals& CurrentSignals,
				int32 SignalMask
			)
			{
				return Consumer->ApplyConsentSignals(
					CurrentSignals,
					SignalMask
				);
			}
		);
	}

	NextStatus.LastUpdated = FDateTime::UtcNow();
	ConsentSignalDeliveryStatus = MoveTemp(NextStatus);
	LastPropagatedConsentSignals = Signals;
	bConsentSignalsPropagated = true;
	BroadcastConsentSignalDeliveryStatus();
}

void UOpenMobileAdsSubsystem::BroadcastConsentSignalDeliveryStatus()
{
	check(IsInGameThread());
	NativeConsentSignalDeliveryChanged.Broadcast(ConsentSignalDeliveryStatus);
	OnConsentSignalDeliveryChanged.Broadcast(ConsentSignalDeliveryStatus);
}

FOpenMobileAdsCanRequestAdsResult UOpenMobileAdsSubsystem::CanRequestAds() const
{
	return EvaluateCanRequestAds(nullptr);
}

FOpenMobileAdsCanRequestAdsResult UOpenMobileAdsSubsystem::EvaluateCanRequestAds(
	IOpenMobileAdsProvider* KnownProvider
) const
{
	FOpenMobileAdsCanRequestAdsContext Context;
	Context.ServiceState = ServiceState;
	Context.Provider = SelectedProviderName;
	Context.ConsentStatus = PrivacySnapshot.ConsentStatus;
	Context.ConsentActivity = PrivacySnapshot.ConsentActivity;
	Context.ConsentRequestState = PrivacySnapshot.ConsentRequestState;
	Context.UsPrivacy = PrivacySnapshot.UsPrivacy;
	Context.bConsentStatusFresh =
		PrivacySnapshot.IsConsentStatusFreshAt(FDateTime::UtcNow());
	IOpenMobileAdsProvider* Provider = nullptr;
	if (ServiceState == EOpenMobileAdsServiceState::Ready)
	{
		Provider = KnownProvider;
		if (!Provider)
		{
			Provider = OpenMobileAdsPrivate::FindRegisteredProvider(
				SelectedProviderName
			);
		}
		if (Provider && Provider->GetProviderName() == SelectedProviderName)
		{
			Context.bProviderAvailable = true;
		}
	}
	FOpenMobileAdsCanRequestAdsResult Decision =
		FOpenMobileAdsCanRequestPolicy::Evaluate(Context);
	if (!Decision.bCanRequestAds || !Provider)
	{
		return Decision;
	}

	const FOpenMobileAdsProviderRequestContext ProviderContext =
		OpenMobileAdsPrivate::MakeProviderPrivacyContext(PrivacySnapshot);
	Context.ProviderPolicy = Provider->GetRequestPolicy(ProviderContext);
	return FOpenMobileAdsCanRequestPolicy::Evaluate(Context);
}

void UOpenMobileAdsSubsystem::RefreshCanRequestAdsDecision()
{
	check(IsInGameThread());
	FOpenMobileAdsCanRequestAdsResult Decision = CanRequestAds();
	PrivacySnapshot.bCanRequestAds = Decision.bCanRequestAds;
	if (
		bCanRequestAdsDecisionInitialized
		&& Decision == LastCanRequestAdsDecision
	)
	{
		return;
	}
	LastCanRequestAdsDecision = MoveTemp(Decision);
	bCanRequestAdsDecisionInitialized = true;
	NativeCanRequestAdsChanged.Broadcast(LastCanRequestAdsDecision);
	OnCanRequestAdsChanged.Broadcast(LastCanRequestAdsDecision);
}

void UOpenMobileAdsSubsystem::EnsureRuntime()
{
	if (!bPrivacySnapshotInitialized)
	{
		const FOpenMobileAdsPrivacyConfiguration& Privacy =
			GetDefault<UOpenMobileAdsSettings>()->Privacy;
		PrivacySnapshot.ConsentStatus = Privacy.bDelayProviderInitializationUntilConsent
			? EOpenMobileAdsConsentStatus::Unknown
			: EOpenMobileAdsConsentStatus::NotRequired;
		PrivacySnapshot.ConsentRequirement =
			Privacy.bDelayProviderInitializationUntilConsent
				? EOpenMobileAdsConsentRequirement::Unknown
				: EOpenMobileAdsConsentRequirement::NotRequired;
		PrivacySnapshot.ConsentRequestState =
			Privacy.bDelayProviderInitializationUntilConsent
				? EOpenMobileAdsConsentRequestState::Unknown
				: EOpenMobileAdsConsentRequestState::Allowed;
		PrivacySnapshot.bConsentStatusFresh =
			!Privacy.bDelayProviderInitializationUntilConsent;
		PrivacySnapshot.ChildDirectedTreatment = Privacy.ChildDirectedTreatment;
		PrivacySnapshot.UnderAgeOfConsent = Privacy.UnderAgeOfConsent;
		PrivacySnapshot.bCanRequestAds = false;
		PrivacySnapshot.Source = TEXT("ProjectSettings");
		PrivacySnapshot.LastUpdated = FDateTime::UtcNow();
		bPrivacySnapshotInitialized = true;
	}
	if (bRuntimeInitialized || bDeinitialized)
	{
		return;
	}

	if (!CacheClock)
	{
		CacheClock = OpenMobileAdsCreateClock();
	}
	if (!RetryRandomSource)
	{
		RetryRandomSource = OpenMobileAdsCreateRetryRandomSource();
	}
	if (!RetryScheduler)
	{
		RetryScheduler = OpenMobileAdsCreateRetryScheduler();
	}
	if (!FrequencyCapTracker)
	{
		FrequencyCapTracker = MakeShared<FOpenMobileAdsFrequencyCapTracker>(
			OpenMobileAdsCreateFrequencyCapStore(!GIsAutomationTesting)
		);
		FrequencyCapTracker->Initialize(
			GetCacheUtcNow(),
			GetCacheMonotonicSeconds()
		);
	}
	if (!CooldownTracker)
	{
		CooldownTracker = MakeShared<FOpenMobileAdsCooldownTracker>();
	}
	EventDispatcher = MakeShared<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe>(*this);
	UGameInstance* GameInstance = GetGameInstance();
	check(GameInstance);
	FullscreenLifecycle = MakeShared<FOpenMobileAdsFullscreenLifecycleCoordinator>(
		OpenMobileAdsCreateUnrealFullscreenLifecycleTarget(*GameInstance)
	);
	ProviderUnregisteredHandle = IModularFeatures::Get().OnModularFeatureUnregistered().AddUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleProviderUnregistered
	);
	NetworkConnectionChangedHandle = FCoreDelegates::OnNetworkConnectionChanged.AddUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleNetworkConnectionChanged
	);
	ApplicationWillDeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleApplicationWillDeactivate
	);
	ApplicationHasReactivatedHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleApplicationHasReactivated
	);
	ApplicationWillEnterBackgroundHandle =
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
			this,
			&UOpenMobileAdsSubsystem::HandleApplicationWillEnterBackground
		);
	ApplicationHasEnteredForegroundHandle =
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
			this,
			&UOpenMobileAdsSubsystem::HandleApplicationHasEnteredForeground
		);
	HandleNetworkConnectionChanged(FPlatformMisc::GetNetworkConnectionType());
	bApplicationActive = true;
	bApplicationInForeground = true;
	bRuntimeInitialized = true;
	RefreshTrackingAuthorizationStatus();
}

void UOpenMobileAdsSubsystem::RefreshTrackingAuthorizationStatus()
{
	check(IsInGameThread());
	ApplyTrackingAuthorizationStatus(
		FOpenMobileAdsTrackingAuthorizationPlatform::GetStatus()
	);
}

bool UOpenMobileAdsSubsystem::IsAdvertisingIdentifierAvailable() const
{
	return IsInGameThread()
		&& FOpenMobileAdsTrackingAuthorizationPlatform::
			IsAdvertisingIdentifierAvailable();
}

void UOpenMobileAdsSubsystem::ApplyTrackingAuthorizationStatus(
	const EOpenMobileAdsTrackingAuthorizationStatus Status
)
{
	check(IsInGameThread());
	if (
		bTrackingAuthorizationStatusInitialized
		&& TrackingAuthorizationStatus == Status
	)
	{
		return;
	}
	TrackingAuthorizationStatus = Status;
	bTrackingAuthorizationStatusInitialized = true;
	NativeTrackingAuthorizationStatusChanged.Broadcast(Status);
	OnTrackingAuthorizationStatusChanged.Broadcast(Status);
}

FOpenMobileAdsOperationResult
UOpenMobileAdsSubsystem::RequestTrackingAuthorization()
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				NAME_None,
				EOpenMobileAdsFailureStage::TrackingAuthorization
			)
		);
	}
	EnsureRuntime();
	if (bDeinitialized)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::TrackingAuthorization,
			NAME_None,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}
	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	if (!Settings->bEnableTrackingAuthorization)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::NotConfigured,
			EOpenMobileAdsFailureStage::TrackingAuthorization,
			NAME_None,
			TEXT("Tracking authorization is disabled in OpenMobile Ads settings."),
			NAME_None,
			TEXT("Enable App Tracking Transparency before requesting authorization.")
		));
	}
	if (!UOpenMobileAdsSettings::IsValidTrackingUsageDescription(
		Settings->TrackingUsageDescription
	))
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::NotConfigured,
			EOpenMobileAdsFailureStage::TrackingAuthorization,
			NAME_None,
			TEXT("Tracking authorization requires a valid usage description."),
			NAME_None,
			TEXT("Set a project-specific Tracking Usage Description before packaging.")
		));
	}
	if (!FOpenMobileAdsTrackingAuthorizationPlatform::IsAvailable())
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::UnsupportedPlatform,
			EOpenMobileAdsFailureStage::TrackingAuthorization,
			NAME_None,
			TEXT("Tracking authorization is unavailable on this platform.")
		));
	}
	if (ActiveTrackingAuthorizationRequestId.IsValid())
	{
		return FOpenMobileAdsOperationResult::Accepted(
			ActiveTrackingAuthorizationRequestId
		);
	}
	RefreshTrackingAuthorizationStatus();
	if (
		TrackingAuthorizationStatus
			!= EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined
	)
	{
		return FOpenMobileAdsOperationResult::Accepted(FGuid::NewGuid());
	}
	if (
		!bApplicationActive
		|| !bApplicationInForeground
		|| ActiveConsentRequestId.IsValid()
		|| !FullscreenLifecycle
		|| FullscreenLifecycle->IsOccupied()
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Busy,
			EOpenMobileAdsFailureStage::TrackingAuthorization,
			NAME_None,
			TEXT("Tracking authorization requires an active foreground application with no other full-screen surface."),
			NAME_None,
			TEXT("Retry after the application is active and the current privacy or ad surface closes."),
			true
		));
	}
	const FGuid RequestId = FGuid::NewGuid();
	if (
		!FullscreenLifecycle->TryReserve(
			EOpenMobileAdsFullscreenSurface::TrackingAuthorization,
			RequestId
		)
		|| !FullscreenLifecycle->BeginPresentation(
			EOpenMobileAdsFullscreenSurface::TrackingAuthorization,
			RequestId
		)
	)
	{
		FullscreenLifecycle->End(
			EOpenMobileAdsFullscreenSurface::TrackingAuthorization,
			RequestId
		);
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Busy,
			EOpenMobileAdsFailureStage::TrackingAuthorization,
			NAME_None,
			TEXT("Tracking authorization could not reserve the full-screen surface."),
			NAME_None,
			FString(),
			true
		));
	}
	ActiveTrackingAuthorizationRequestId = RequestId;
	const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
	FString NativeError;
	if (!FOpenMobileAdsTrackingAuthorizationPlatform::RequestAuthorization(
		[WeakThis, RequestId](
			EOpenMobileAdsTrackingAuthorizationStatus Status
		)
		{
			if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->HandleTrackingAuthorizationCompleted(
					RequestId,
					Status
				);
			}
		},
		NativeError
	))
	{
		ActiveTrackingAuthorizationRequestId.Invalidate();
		FullscreenLifecycle->End(
			EOpenMobileAdsFullscreenSurface::TrackingAuthorization,
			RequestId
		);
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::NativeFailure,
			EOpenMobileAdsFailureStage::TrackingAuthorization,
			NAME_None,
			NativeError.IsEmpty()
				? TEXT("The platform rejected the tracking authorization request.")
				: MoveTemp(NativeError),
			NAME_None,
			FString(),
			true
		));
	}
	return FOpenMobileAdsOperationResult::Accepted(RequestId);
}

void UOpenMobileAdsSubsystem::HandleTrackingAuthorizationCompleted(
	const FGuid RequestId,
	const EOpenMobileAdsTrackingAuthorizationStatus Status
)
{
	check(IsInGameThread());
	if (
		bDeinitialized
		|| RequestId != ActiveTrackingAuthorizationRequestId
	)
	{
		return;
	}
	ActiveTrackingAuthorizationRequestId.Invalidate();
	if (FullscreenLifecycle)
	{
		FullscreenLifecycle->End(
			EOpenMobileAdsFullscreenSurface::TrackingAuthorization,
			RequestId
		);
	}
	ApplyTrackingAuthorizationStatus(Status);
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::UpdatePrivacySnapshot(
	FOpenMobileAdsPrivacySnapshot Snapshot
)
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				NAME_None,
				EOpenMobileAdsFailureStage::Consent
			)
		);
	}
	if (bDeinitialized)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Consent,
			NAME_None,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}
	if (
		bChildDirectedTreatmentLocked
		&& Snapshot.ChildDirectedTreatment
			!= PrivacySnapshot.ChildDirectedTreatment
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Consent,
			NAME_None,
			TEXT("Child-directed treatment cannot change after provider initialization starts."),
			SelectedProviderName,
			TEXT("Set child-directed treatment before initializing ads, or restart the ads subsystem with the new value.")
		));
	}
	if (
		bUnderAgeOfConsentLocked
		&& Snapshot.UnderAgeOfConsent != PrivacySnapshot.UnderAgeOfConsent
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Consent,
			NAME_None,
			TEXT("Under-age-of-consent treatment cannot change after provider initialization starts."),
			SelectedProviderName,
			TEXT("Set under-age-of-consent treatment before initializing ads, or restart the ads subsystem with the new value.")
		));
	}
	if (Snapshot.LastUpdated == FDateTime())
	{
		Snapshot.LastUpdated = FDateTime::UtcNow();
	}
	PrivacySnapshot = MoveTemp(Snapshot);
	bPrivacySnapshotInitialized = true;
	if (bProviderInitializationStarted)
	{
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsPrivate::FindRegisteredProvider(SelectedProviderName))
		{
			PropagateConsentSignals(*Provider, true);
		}
	}
	BroadcastConsentStatus();
	return FOpenMobileAdsOperationResult::Accepted(FGuid());
}

void UOpenMobileAdsSubsystem::HandleConsentRefreshCompleted(
	FGuid RequestId,
	FName AdsProviderName,
	FName ConsentProviderName,
	FOpenMobileAdsConsentStatusUpdate Update
)
{
	check(IsInGameThread());
	if (
		bDeinitialized
		|| RequestId != ActiveConsentRequestId
		|| AdsProviderName != ActiveConsentAdsProviderName
		|| ConsentProviderName != ActiveConsentProviderName
	)
	{
		return;
	}
	if (ConsentOperationSink)
	{
		ConsentOperationSink->Invalidate();
		ConsentOperationSink.Reset();
	}
	if (Update.Type != EOpenMobileAdsConsentStatusUpdateType::Completed)
	{
		HandleConsentOperationFailed(
			RequestId,
			AdsProviderName,
			ConsentProviderName,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderFailure,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The consent provider returned an invalid refresh result."),
				ConsentProviderName
			)
		);
		return;
	}
	if (Update.Source.IsNone())
	{
		Update.Source = ConsentProviderName;
	}
	if (Update.Status != EOpenMobileAdsConsentStatus::Required)
	{
		ClearConsentOperation(false);
		ApplyConsentStatusUpdateOnGameThread(MoveTemp(Update));
		return;
	}

	ApplyConsentStatusUpdateOnGameThread(MoveTemp(Update));
	if (
		bDeinitialized
		|| RequestId != ActiveConsentRequestId
		|| AdsProviderName != ActiveConsentAdsProviderName
	)
	{
		return;
	}
	IOpenMobileAdsProvider* Provider =
		OpenMobileAdsPrivate::FindRegisteredProvider(AdsProviderName);
	if (
		!Provider
		|| !Provider->IsSupported()
		|| Provider->GetConsentProviderName() != ConsentProviderName
	)
	{
		HandleConsentOperationFailed(
			RequestId,
			AdsProviderName,
			ConsentProviderName,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The consent provider became unavailable before form presentation."),
				ConsentProviderName,
				TEXT("Keep the selected provider enabled until consent gathering finishes.")
			)
		);
		return;
	}
	StartConsentForm(*Provider, false);
}

bool UOpenMobileAdsSubsystem::StartConsentForm(
	IOpenMobileAdsProvider& Provider,
	bool bPrivacyOptions
)
{
	check(IsInGameThread());
	const FGuid RequestId = ActiveConsentRequestId;
	const FName AdsProviderName = ActiveConsentAdsProviderName;
	const FName ConsentProviderName = ActiveConsentProviderName;
	if (
		!RequestId.IsValid()
		|| !FullscreenLifecycle
		|| !FullscreenLifecycle->TryReserve(
			EOpenMobileAdsFullscreenSurface::Consent,
			RequestId
		)
		|| !FullscreenLifecycle->BeginPresentation(
			EOpenMobileAdsFullscreenSurface::Consent,
			RequestId
		)
	)
	{
		if (FullscreenLifecycle)
		{
			FullscreenLifecycle->End(
				EOpenMobileAdsFullscreenSurface::Consent,
				RequestId
			);
		}
		HandleConsentOperationFailed(
			RequestId,
			AdsProviderName,
			ConsentProviderName,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Busy,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				bPrivacyOptions
					? TEXT("The privacy-options form conflicts with another full-screen surface.")
					: TEXT("The required consent form conflicts with another full-screen surface."),
				ConsentProviderName,
				TEXT("Retry consent after the current full-screen surface closes."),
				true
			)
		);
		return false;
	}

	ApplyConsentStatusUpdateOnGameThread(
		FOpenMobileAdsConsentStatusUpdate::BeginFormPresentation(
			ConsentProviderName
		)
	);
	const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
	const TSharedRef<OpenMobileAdsPrivate::FConsentProviderSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FConsentProviderSink, ESPMode::ThreadSafe>(
			[WeakThis, RequestId, AdsProviderName, ConsentProviderName](
				FOpenMobileAdsConsentStatusUpdate Update
			) mutable
			{
				AsyncTask(
					ENamedThreads::GameThread,
					[WeakThis, RequestId, AdsProviderName, ConsentProviderName,
						Update = MoveTemp(Update)]() mutable
					{
						if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
						{
							Subsystem->HandleConsentFormCompleted(
								RequestId,
								AdsProviderName,
								ConsentProviderName,
								MoveTemp(Update)
							);
						}
					}
				);
			},
			[WeakThis, RequestId, AdsProviderName, ConsentProviderName](
				FOpenMobileAdsError Error
			) mutable
			{
				AsyncTask(
					ENamedThreads::GameThread,
					[WeakThis, RequestId, AdsProviderName, ConsentProviderName,
						Error = MoveTemp(Error)]() mutable
					{
						if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
						{
							Subsystem->HandleConsentOperationFailed(
								RequestId,
								AdsProviderName,
								ConsentProviderName,
								MoveTemp(Error)
							);
						}
					}
				);
			}
		);
	ConsentOperationSink = Sink;
	FOpenMobileAdsError ProviderError;
	const bool bStarted = bPrivacyOptions
		? Provider.PresentPrivacyOptionsForm(
			ActiveConsentRequest,
			Sink,
			ProviderError
		)
		: Provider.PresentRequiredConsentForm(
			ActiveConsentRequest,
			Sink,
			ProviderError
		);
	if (!bStarted)
	{
		Sink->Invalidate();
		ConsentOperationSink.Reset();
		FOpenMobileAdsError Error = OpenMobileAdsPrivate::NormalizeConsentError(
			MoveTemp(ProviderError),
			ConsentProviderName
		);
		ClearConsentOperation(true);
		ApplyConsentStatusUpdateOnGameThread(
			FOpenMobileAdsConsentStatusUpdate::Fail(
				ConsentProviderName,
				MoveTemp(Error)
			)
		);
		return false;
	}
	Sink->Commit();
	return true;
}

void UOpenMobileAdsSubsystem::HandleConsentFormCompleted(
	FGuid RequestId,
	FName AdsProviderName,
	FName ConsentProviderName,
	FOpenMobileAdsConsentStatusUpdate Update
)
{
	check(IsInGameThread());
	if (
		bDeinitialized
		|| RequestId != ActiveConsentRequestId
		|| AdsProviderName != ActiveConsentAdsProviderName
		|| ConsentProviderName != ActiveConsentProviderName
	)
	{
		return;
	}
	if (Update.Type != EOpenMobileAdsConsentStatusUpdateType::Completed)
	{
		HandleConsentOperationFailed(
			RequestId,
			AdsProviderName,
			ConsentProviderName,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderFailure,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The consent provider returned an invalid form result."),
				ConsentProviderName
			)
		);
		return;
	}
	if (Update.Source.IsNone())
	{
		Update.Source = ConsentProviderName;
	}
	ClearConsentOperation(true);
	ApplyConsentStatusUpdateOnGameThread(MoveTemp(Update));
}

void UOpenMobileAdsSubsystem::HandleConsentOperationFailed(
	FGuid RequestId,
	FName AdsProviderName,
	FName ConsentProviderName,
	FOpenMobileAdsError Error
)
{
	check(IsInGameThread());
	if (
		bDeinitialized
		|| RequestId != ActiveConsentRequestId
		|| AdsProviderName != ActiveConsentAdsProviderName
		|| ConsentProviderName != ActiveConsentProviderName
	)
	{
		return;
	}
	const bool bEndPresentation =
		PrivacySnapshot.ConsentActivity
			== EOpenMobileAdsConsentActivity::PresentingForm;
	Error = OpenMobileAdsPrivate::NormalizeConsentError(
		MoveTemp(Error),
		ConsentProviderName
	);
	ClearConsentOperation(bEndPresentation);
	ApplyConsentStatusUpdateOnGameThread(
		FOpenMobileAdsConsentStatusUpdate::Fail(
			ConsentProviderName,
			MoveTemp(Error)
		)
	);
}

void UOpenMobileAdsSubsystem::ClearConsentOperation(bool bEndPresentation)
{
	check(IsInGameThread());
	if (bEndPresentation && FullscreenLifecycle)
	{
		FullscreenLifecycle->End(
			EOpenMobileAdsFullscreenSurface::Consent,
			ActiveConsentRequestId
		);
	}
	if (ConsentOperationSink)
	{
		ConsentOperationSink->Invalidate();
		ConsentOperationSink.Reset();
	}
	ActiveConsentRequestId.Invalidate();
	ActiveConsentRequest = FOpenMobileAdsConsentRequest();
	ActiveConsentAdsProviderName = NAME_None;
	ActiveConsentProviderName = NAME_None;
	bPrivacyOptionsPresentationActive = false;
}

void UOpenMobileAdsSubsystem::ApplyConsentStatusUpdate(
	FOpenMobileAdsConsentStatusUpdate Update
)
{
	if (IsInGameThread())
	{
		ApplyConsentStatusUpdateOnGameThread(MoveTemp(Update));
		return;
	}

	const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
	AsyncTask(
		ENamedThreads::GameThread,
		[WeakThis, Update = MoveTemp(Update)]() mutable
		{
			if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->ApplyConsentStatusUpdateOnGameThread(MoveTemp(Update));
			}
		}
	);
}

void UOpenMobileAdsSubsystem::ApplyConsentStatusUpdateOnGameThread(
	FOpenMobileAdsConsentStatusUpdate Update
)
{
	check(IsInGameThread());
	if (bDeinitialized)
	{
		return;
	}
	if (!Update.Source.IsNone())
	{
		PrivacySnapshot.Source = Update.Source;
	}

	switch (Update.Type)
	{
	case EOpenMobileAdsConsentStatusUpdateType::RefreshStarted:
		PrivacySnapshot.ConsentActivity = EOpenMobileAdsConsentActivity::Refreshing;
		PrivacySnapshot.Error = FOpenMobileAdsError();
		break;

	case EOpenMobileAdsConsentStatusUpdateType::FormPresentationStarted:
		PrivacySnapshot.ConsentActivity =
			EOpenMobileAdsConsentActivity::PresentingForm;
		PrivacySnapshot.Error = FOpenMobileAdsError();
		break;

	case EOpenMobileAdsConsentStatusUpdateType::ResetStarted:
		PrivacySnapshot.ConsentStatus = EOpenMobileAdsConsentStatus::Unknown;
		PrivacySnapshot.ConsentActivity = EOpenMobileAdsConsentActivity::Resetting;
		PrivacySnapshot.GdprApplicability =
			EOpenMobileAdsGdprApplicability::Unknown;
		PrivacySnapshot.ConsentRequirement =
			EOpenMobileAdsConsentRequirement::Unknown;
		PrivacySnapshot.ConsentRequestState =
			EOpenMobileAdsConsentRequestState::Unknown;
		PrivacySnapshot.UsPrivacy = FOpenMobileAdsUsPrivacyState();
		PrivacySnapshot.bConsentStatusFresh = false;
		PrivacySnapshot.ConsentExpiresAt = FDateTime();
		PrivacySnapshot.bRestoredFromProviderStorage = false;
		PrivacySnapshot.ProviderDetails = FOpenMobileAdsConsentProviderDetails();
		PrivacySnapshot.Error = FOpenMobileAdsError();
		break;

	case EOpenMobileAdsConsentStatusUpdateType::Completed:
		PrivacySnapshot.ConsentStatus = Update.Status;
		PrivacySnapshot.ConsentActivity = EOpenMobileAdsConsentActivity::Idle;
		PrivacySnapshot.GdprApplicability = Update.GdprApplicability;
		PrivacySnapshot.ConsentRequirement = Update.Requirement;
		PrivacySnapshot.ConsentRequestState = Update.RequestState;
		PrivacySnapshot.UsPrivacy = Update.UsPrivacy;
		PrivacySnapshot.bConsentStatusFresh = Update.bStatusFresh;
		PrivacySnapshot.ConsentExpiresAt = Update.ExpiresAt;
		PrivacySnapshot.bRestoredFromProviderStorage =
			Update.bRestoredFromProviderStorage;
		PrivacySnapshot.ProviderDetails = MoveTemp(Update.ProviderDetails);
		if (!PrivacySnapshot.ProviderDetails.bIsAvailable)
		{
			PrivacySnapshot.ProviderDetails.RawStatus.Reset();
			PrivacySnapshot.ProviderDetails.RawMessage.Reset();
		}
		PrivacySnapshot.Error = FOpenMobileAdsError();
		break;

	case EOpenMobileAdsConsentStatusUpdateType::Failed:
		PrivacySnapshot.ConsentActivity = EOpenMobileAdsConsentActivity::Idle;
		PrivacySnapshot.Error = OpenMobileAdsPrivate::NormalizeConsentError(
			MoveTemp(Update.Error),
			PrivacySnapshot.Source
		);
		break;
	}

	PrivacySnapshot.LastUpdated = FDateTime::UtcNow();
	bPrivacySnapshotInitialized = true;
	if (
		Update.Type == EOpenMobileAdsConsentStatusUpdateType::Completed
		&& bProviderInitializationStarted
	)
	{
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsPrivate::FindRegisteredProvider(SelectedProviderName))
		{
			PropagateConsentSignals(*Provider, true);
		}
	}
	BroadcastConsentStatus();
}

void UOpenMobileAdsSubsystem::HandleNetworkConnectionChanged(
	ENetworkConnectionType ConnectionType
)
{
	const bool bDefinitelyOffline =
		FOpenMobileAdsConnectivityPolicy::IsDefinitelyOffline(ConnectionType);
	bPlatformDefinitelyOffline.Store(bDefinitelyOffline);
	if (IsInGameThread())
	{
		if (bDefinitelyOffline)
		{
			PauseAutomaticPreloads();
		}
		else
		{
			ResumeConnectivityDeferredRetries();
			ReevaluateAutomaticPreloads();
		}
		return;
	}

	const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis, bDefinitelyOffline]()
	{
		if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
		{
			if (bDefinitelyOffline)
			{
				Subsystem->PauseAutomaticPreloads();
			}
			else
			{
				Subsystem->ResumeConnectivityDeferredRetries();
				Subsystem->ReevaluateAutomaticPreloads();
			}
		}
	});
}

void UOpenMobileAdsSubsystem::HandleApplicationWillDeactivate()
{
	bApplicationActive = false;
	PauseAutomaticPreloads();
	if (FullscreenLifecycle)
	{
		FullscreenLifecycle->SetApplicationActive(false);
	}
}

void UOpenMobileAdsSubsystem::HandleApplicationHasReactivated()
{
	bApplicationActive = true;
	if (FullscreenLifecycle)
	{
		FullscreenLifecycle->SetApplicationActive(true);
	}
	RefreshTrackingAuthorizationStatus();
	ExpireCachedAds();
	ReevaluateAutomaticPreloads();
}

void UOpenMobileAdsSubsystem::HandleApplicationWillEnterBackground()
{
	bApplicationInForeground = false;
	PauseAutomaticPreloads();
	if (FullscreenLifecycle)
	{
		FullscreenLifecycle->SetApplicationInForeground(false);
	}
}

void UOpenMobileAdsSubsystem::HandleApplicationHasEnteredForeground()
{
	bApplicationInForeground = true;
	if (FullscreenLifecycle)
	{
		FullscreenLifecycle->SetApplicationInForeground(true);
	}
	ExpireCachedAds();
	ReevaluateAutomaticPreloads();
}

FName UOpenMobileAdsSubsystem::GetPreferredProviderName() const
{
	return GetDefault<UOpenMobileAdsSettings>()->PreferredProvider;
}

IOpenMobileAdsProvider* UOpenMobileAdsSubsystem::FindProvider(
	FOpenMobileAdsError* OutError
) const
{
	const TArray<IOpenMobileAdsProvider*> Providers =
		IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileAdsProvider>(
			IOpenMobileAdsProvider::GetModularFeatureName()
		);
	FOpenMobileAdsProviderSelection Selection =
		FOpenMobileAdsProviderResolver::Resolve(
			Providers,
			SelectedProviderName.IsNone()
				? GetPreferredProviderName()
				: SelectedProviderName
		);
	if (OutError)
	{
		*OutError = MoveTemp(Selection.Error);
	}
	return Selection.Provider;
}

const FOpenMobileAdsPlacementSettings* UOpenMobileAdsSubsystem::FindConfiguredPlacement(
	FName Placement
) const
{
	return GetDefault<UOpenMobileAdsSettings>()->FindPlacement(Placement);
}

FOpenMobileAdsError UOpenMobileAdsSubsystem::ValidatePlacementForProvider(
	FName Placement,
	IOpenMobileAdsProvider*& OutProvider,
	FOpenMobileAdsResolvedPlacement& OutPlacement
) const
{
	OutProvider = nullptr;
	if (Placement.IsNone())
	{
		return FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidPlacement,
			EOpenMobileAdsFailureStage::Configuration,
			Placement,
			TEXT("Placement name must not be empty."),
			NAME_None,
			TEXT("Pass a configured placement name.")
		);
	}

	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	const FOpenMobileAdsPlacementSettings* Configuration = Settings->FindPlacement(Placement);
	if (!Configuration)
	{
		return FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::UnknownPlacement,
			EOpenMobileAdsFailureStage::Configuration,
			Placement,
			TEXT("The requested ads placement is not configured."),
			NAME_None,
			TEXT("Add the placement in OpenMobile Ads project settings.")
		);
	}

	const TArray<FOpenMobileAdsConfigurationIssue> Issues =
		FOpenMobileAdsConfigurationValidator::Validate(Settings->Placements);
	for (const FOpenMobileAdsConfigurationIssue& Issue : Issues)
	{
		if (
			Issue.Severity == EOpenMobileAdsConfigurationIssueSeverity::Error
			&& (Issue.Placement == Placement || Issue.ConflictingPlacement == Placement)
		)
		{
			return FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::InvalidPlacement,
				EOpenMobileAdsFailureStage::Configuration,
				Placement,
				Issue.Message,
				NAME_None,
				TEXT("Correct the placement in OpenMobile Ads project settings.")
			);
		}
	}

	OutPlacement = Configuration->Resolve(OpenMobileAdsGetCurrentPlatform());
	if (!OutPlacement.bEnabled)
	{
		return FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::DisabledPlacement,
			EOpenMobileAdsFailureStage::Configuration,
			Placement,
			TEXT("The requested ads placement is disabled."),
			NAME_None,
			TEXT("Enable the placement for the current platform.")
		);
	}
	if (ServiceState != EOpenMobileAdsServiceState::Ready)
	{
		return OpenMobileAdsPrivate::MakeServiceNotReadyError(
			Placement,
			ServiceState,
			InitializationError
		);
	}

	FOpenMobileAdsError ProviderError;
	OutProvider = FindProvider(&ProviderError);
	return ProviderError;
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::LoadAd(
	FName Placement,
	FOpenMobileAdsLoadOptions Options
)
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				Placement,
				EOpenMobileAdsFailureStage::Load
			)
		);
	}
	EnsureRuntime();
	if (bDeinitialized || !EventDispatcher)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Load,
			Placement,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}

	IOpenMobileAdsProvider* Provider = nullptr;
	FOpenMobileAdsResolvedPlacement ResolvedPlacement;
	FOpenMobileAdsError Error = ValidatePlacementForProvider(
		Placement,
		Provider,
		ResolvedPlacement
	);
	if (Error.IsSet())
	{
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}
	const FOpenMobileAdsCanRequestAdsResult RequestDecision =
		EvaluateCanRequestAds(Provider);
	if (!RequestDecision.bCanRequestAds)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::PrivacyBlocked,
			EOpenMobileAdsFailureStage::Consent,
			Placement,
			RequestDecision.Explanation,
			Provider->GetProviderName(),
			TEXT("Wait for the consent source to allow ad requests before loading this placement.")
		));
	}

	const FOpenMobileAdsProviderCapabilities ProviderCapabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* FormatCapabilities =
		ProviderCapabilities.FindFormat(ResolvedPlacement.Format);
	if (!FormatCapabilities || !FormatCapabilities->bCanLoad)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeUnsupportedFormatError(
				Placement,
				Provider->GetProviderName(),
				EOpenMobileAdsFailureStage::Load
			)
		);
	}
	if (FormatCapabilities->MaxCachedAdsPerPlacement < 1)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderFailure,
			EOpenMobileAdsFailureStage::Load,
			Placement,
			TEXT("The selected ads provider reports no cache capacity for this format."),
			Provider->GetProviderName(),
			TEXT("Advertise at least one cached ad for formats that support loading.")
		));
	}
	if (
		!FMath::IsFinite(FormatCapabilities->CacheLifetimeSeconds)
		|| FormatCapabilities->CacheLifetimeSeconds < 0.0
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderFailure,
			EOpenMobileAdsFailureStage::Load,
			Placement,
			TEXT("The selected ads provider reports an invalid cache lifetime."),
			Provider->GetProviderName(),
			TEXT("Advertise zero for provider-managed expiration or a finite positive lifetime.")
		));
	}

	FOpenMobileAdsPlacementStatus* ExistingStatus = PlacementStatuses.Find(Placement);
	if (ExistingStatus)
	{
		const bool bBusy = ExistingStatus->State == EOpenMobileAdPlacementState::Showing
			|| ExistingStatus->State == EOpenMobileAdPlacementState::Hiding
			|| ExistingStatus->State == EOpenMobileAdPlacementState::Destroying;
		const bool bRequiresReload =
			ExistingStatus->State == EOpenMobileAdPlacementState::Loading
			|| ExistingStatus->State == EOpenMobileAdPlacementState::Ready
			|| (
				ExistingStatus->State == EOpenMobileAdPlacementState::Hidden
				&& ExistingStatus->CachedAdId.IsValid()
			);
		if (bBusy || (bRequiresReload && !Options.bForceReload))
		{
			return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Busy,
				EOpenMobileAdsFailureStage::Load,
				Placement,
				bBusy || ExistingStatus->State == EOpenMobileAdPlacementState::Loading
					? TEXT("The placement already has an operation in progress.")
					: TEXT("The placement already has a ready ad."),
				Provider->GetProviderName()
			));
		}
	}
	if (bPlatformDefinitelyOffline.Load())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOfflineError(
				Placement,
				Provider->GetProviderName(),
				EOpenMobileAdsFailureStage::Load
			)
		);
	}

	const bool bHadStatus = ExistingStatus != nullptr;
	const FOpenMobileAdsPlacementStatus PreviousStatus = bHadStatus
		? *ExistingStatus
		: FOpenMobileAdsPlacementStatus();
	const FGuid SupersededRequestId =
		bHadStatus
		&& PreviousStatus.State == EOpenMobileAdPlacementState::Loading
		&& Options.bForceReload
			? PreviousStatus.ActiveRequestId
			: FGuid();
	bool bHasCompletionFallback = bHadStatus;
	FOpenMobileAdsPlacementStatus CompletionFallback = PreviousStatus;
	if (SupersededRequestId.IsValid())
	{
		bHasCompletionFallback = false;
		const TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>*
			SupersededContext = ActiveRequests.Find(SupersededRequestId);
		if (SupersededContext && SupersededContext->IsValid())
		{
			if (const FOpenMobileAdsPlacementStatus* ReadyFallback =
				(*SupersededContext)->PreviousStatuses.Find(Placement))
			{
				CompletionFallback = *ReadyFallback;
				bHasCompletionFallback =
					OpenMobileAdsPrivate::HasReusableCachedAdState(
						ReadyFallback->State
					);
			}
		}
	}
	FOpenMobileAdsPlacementStatus& Status = PlacementStatuses.FindOrAdd(Placement);
	Status.Placement = Placement;
	Status.Format = ResolvedPlacement.Format;
	Status.State = EOpenMobileAdPlacementState::Loading;
	Status.Provider = Provider->GetProviderName();
	Status.ActiveRequestId = FGuid::NewGuid();
	Status.LastError = FOpenMobileAdsError();

	FOpenMobileAdsLoadRequest Request;
	Request.RequestId = Status.ActiveRequestId;
	Request.Placement = MoveTemp(ResolvedPlacement);
	Request.Options = MoveTemp(Options);
	Request.PrivacyContext =
		OpenMobileAdsPrivate::MakeProviderPrivacyContext(PrivacySnapshot);
	const TSharedRef<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe>(
			EventDispatcher.ToSharedRef(),
			Status.Provider,
			Placement,
			Status.Format,
			EOpenMobileAdsFailureStage::Load,
			Status.ActiveRequestId
		);
	TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		MakeShared<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>();
	Context->Placement = Placement;
	Context->Provider = Status.Provider;
	Context->Format = Status.Format;
	Context->Stage = EOpenMobileAdsFailureStage::Load;
	Context->EventSink = Sink;
	Context->LoadRequest = Request;
	if (const FOpenMobileAdsPlacementSettings* Configuration =
		FindConfiguredPlacement(Placement))
	{
		Context->PlacementMaxRetryAttempts =
			Configuration->MaxRetryAttempts;
	}
	if (bHasCompletionFallback)
	{
		Context->PreviousStatuses.Add(Placement, CompletionFallback);
	}
	ActiveRequests.Add(Status.ActiveRequestId, Context);

	if (!Provider->Load(Request, Sink, Error))
	{
		Sink->Invalidate();
		ActiveRequests.Remove(Status.ActiveRequestId);
		if (bHadStatus)
		{
			PlacementStatuses[Placement] = PreviousStatus;
		}
		else
		{
			PlacementStatuses.Remove(Placement);
		}
		Error = OpenMobileAdsPrivate::NormalizeProviderError(
			MoveTemp(Error),
			EOpenMobileAdsFailureStage::Load,
			Placement,
			Provider->GetProviderName(),
			TEXT("The ads provider rejected the load request without a typed error.")
		);
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}
	CancelSupersededRequest(SupersededRequestId);
	CancelAutomaticPreload(Placement);

	FOpenMobileAdsEvent Started;
	Started.Type = EOpenMobileAdsEventType::LoadStarted;
	Started.Placement = Placement;
	Started.Format = Status.Format;
	Started.PlacementState = Status.State;
	Started.Provider = Status.Provider;
	Started.RequestId = Status.ActiveRequestId;
	SubmitServiceEvent(MoveTemp(Started));
	Sink->Commit();
	return FOpenMobileAdsOperationResult::Accepted(Status.ActiveRequestId);
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::ReloadAd(FName Placement)
{
	FOpenMobileAdsLoadOptions Options;
	Options.bForceReload = true;
	return LoadAd(Placement, MoveTemp(Options));
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::ShowAd(
	FName Placement,
	FOpenMobileAdsShowOptions Options
)
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				Placement,
				EOpenMobileAdsFailureStage::Show
			)
		);
	}
	EnsureRuntime();
	if (bDeinitialized || !EventDispatcher)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Show,
			Placement,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}

	IOpenMobileAdsProvider* Provider = nullptr;
	FOpenMobileAdsResolvedPlacement ResolvedPlacement;
	FOpenMobileAdsError Error = ValidatePlacementForProvider(
		Placement,
		Provider,
		ResolvedPlacement
	);
	if (Error.IsSet())
	{
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}

	const FOpenMobileAdsCanShowResult Decision = EvaluateCanShow(
		Placement,
		Provider,
		&ResolvedPlacement
	);
	if (!Decision.bCanShow)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeShowPolicyError(
				Placement,
				Provider->GetProviderName(),
				Decision
			)
		);
	}

	FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement);
	check(Status);

	const FOpenMobileAdsPlacementStatus PreviousStatus = *Status;
	Status->State = EOpenMobileAdPlacementState::Showing;
	Status->ActiveRequestId = FGuid::NewGuid();
	FOpenMobileAdsShowRequest Request;
	Request.RequestId = Status->ActiveRequestId;
	Request.CachedAdId = Status->CachedAdId;
	Request.Placement = Placement;
	Request.Format = Status->Format;
	Request.Options = MoveTemp(Options);
	const bool bUsesFullscreenLifecycle =
		OpenMobileAdsPrivate::UsesFullscreenLifecycle(Request.Format);
	const bool bReserved = !bUsesFullscreenLifecycle || (FullscreenLifecycle
		&& FullscreenLifecycle->TryReserve(
			EOpenMobileAdsFullscreenSurface::Ad,
			Request.RequestId
		));
	if (
		!bReserved
		|| (
			bUsesFullscreenLifecycle
			&& !FullscreenLifecycle->BeginPresentation(
			EOpenMobileAdsFullscreenSurface::Ad,
			Request.RequestId
			)
		)
	)
	{
		if (bUsesFullscreenLifecycle && bReserved)
		{
			FullscreenLifecycle->End(
				EOpenMobileAdsFullscreenSurface::Ad,
				Request.RequestId
			);
		}
		*Status = PreviousStatus;
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Show,
			Placement,
			TEXT("Another full-screen surface owns the application lifecycle."),
			Provider->GetProviderName(),
			TEXT("Wait for the current ad, consent form, or inspector to close before showing another ad.")
		));
	}
	const TSharedRef<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe>(
			EventDispatcher.ToSharedRef(),
			Status->Provider,
			Placement,
			Status->Format,
			EOpenMobileAdsFailureStage::Show,
			Status->ActiveRequestId,
			Status->CachedAdId,
			ResolvedPlacement.FallbackRewardType,
			ResolvedPlacement.FallbackRewardAmount
		);
	TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		MakeShared<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>();
	Context->Placement = Placement;
	Context->Provider = Status->Provider;
	Context->Format = Status->Format;
	Context->Stage = EOpenMobileAdsFailureStage::Show;
	Context->EventSink = Sink;
	Context->PreviousStatuses.Add(Placement, PreviousStatus);
	ActiveRequests.Add(Status->ActiveRequestId, Context);

	if (!Provider->Show(Request, Sink, Error))
	{
		if (bUsesFullscreenLifecycle)
		{
			FullscreenLifecycle->End(
				EOpenMobileAdsFullscreenSurface::Ad,
				Request.RequestId
			);
		}
		Sink->Invalidate();
		ActiveRequests.Remove(Status->ActiveRequestId);
		*Status = PreviousStatus;
		Error = OpenMobileAdsPrivate::NormalizeProviderError(
			MoveTemp(Error),
			EOpenMobileAdsFailureStage::Show,
			Placement,
			Provider->GetProviderName(),
			TEXT("The ads provider rejected the show request without a typed error.")
		);
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}

	FOpenMobileAdsEvent Accepted;
	Accepted.Type = EOpenMobileAdsEventType::ShowAccepted;
	Accepted.Placement = Placement;
	Accepted.Format = Status->Format;
	Accepted.PlacementState = Status->State;
	Accepted.Provider = Status->Provider;
	Accepted.RequestId = Status->ActiveRequestId;
	Accepted.CachedAdId = Status->CachedAdId;
	SubmitServiceEvent(MoveTemp(Accepted));
	Sink->Commit();
	return FOpenMobileAdsOperationResult::Accepted(Status->ActiveRequestId);
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::HideAd(FName Placement)
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				Placement,
				EOpenMobileAdsFailureStage::Hide
			)
		);
	}
	EnsureRuntime();
	if (bDeinitialized || !EventDispatcher)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Hide,
			Placement,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}

	IOpenMobileAdsProvider* Provider = nullptr;
	FOpenMobileAdsResolvedPlacement ResolvedPlacement;
	FOpenMobileAdsError Error = ValidatePlacementForProvider(
		Placement,
		Provider,
		ResolvedPlacement
	);
	if (Error.IsSet())
	{
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}

	const FOpenMobileAdsProviderCapabilities ProviderCapabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* FormatCapabilities =
		ProviderCapabilities.FindFormat(ResolvedPlacement.Format);
	if (
		OpenMobileAdsPrivate::UsesFullscreenLifecycle(ResolvedPlacement.Format)
		|| !FormatCapabilities
		|| !FormatCapabilities->bCanHide
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeUnsupportedFormatError(
				Placement,
				Provider->GetProviderName(),
				EOpenMobileAdsFailureStage::Hide
			)
		);
	}

	FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement);
	if (!Status)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::NotReady,
			EOpenMobileAdsFailureStage::Hide,
			Placement,
			TEXT("The placement has no ad to hide."),
			Provider->GetProviderName()
		));
	}
	if (Status->State == EOpenMobileAdPlacementState::Hiding)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Busy,
			EOpenMobileAdsFailureStage::Hide,
			Placement,
			TEXT("The placement already has a hide operation in progress."),
			Provider->GetProviderName()
		));
	}
	const bool bAlreadyHidden =
		Status->State == EOpenMobileAdPlacementState::Ready
		|| Status->State == EOpenMobileAdPlacementState::Hidden;
	if (!bAlreadyHidden && Status->State != EOpenMobileAdPlacementState::Showing)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Hide,
			Placement,
			TEXT("The placement is not visible and cannot be hidden."),
			Provider->GetProviderName()
		));
	}
	if (
		Status->State != EOpenMobileAdPlacementState::Hidden
		&& !Status->CachedAdId.IsValid()
	)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderFailure,
			EOpenMobileAdsFailureStage::Hide,
			Placement,
			TEXT("The placement is visible or ready without a valid cache identity."),
			Provider->GetProviderName()
		));
	}

	const FOpenMobileAdsPlacementStatus PreviousStatus = *Status;
	const bool bPreserveCachedAd =
		PreviousStatus.CachedAdId.IsValid()
		&& FormatCapabilities->bPreservesCachedAdOnHide
		&& ResolvedPlacement.HideCachePolicy
			== EOpenMobileAdsHideCachePolicy::PreserveWhenSupported;
	Status->State = EOpenMobileAdPlacementState::Hiding;
	Status->ActiveRequestId = FGuid::NewGuid();
	Status->LastError = FOpenMobileAdsError();
	const FGuid HideRequestId = Status->ActiveRequestId;

	FOpenMobileAdsHideRequest Request;
	Request.RequestId = HideRequestId;
	Request.CachedAdId = PreviousStatus.CachedAdId;
	Request.Placement = Placement;
	Request.Format = Status->Format;
	Request.bPreserveCachedAd = bPreserveCachedAd;
	const TSharedRef<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe>(
			EventDispatcher.ToSharedRef(),
			Status->Provider,
			Placement,
			Status->Format,
			EOpenMobileAdsFailureStage::Hide,
			HideRequestId,
			PreviousStatus.CachedAdId
		);
	TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		MakeShared<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>();
	Context->Placement = Placement;
	Context->Provider = Status->Provider;
	Context->Format = Status->Format;
	Context->Stage = EOpenMobileAdsFailureStage::Hide;
	Context->EventSink = Sink;
	Context->PreviousStatuses.Add(Placement, PreviousStatus);
	Context->bPreserveCachedAdOnHide = bPreserveCachedAd;
	ActiveRequests.Add(HideRequestId, Context);

	if (bAlreadyHidden)
	{
		Context->bProviderAttemptActive = false;
		FOpenMobileAdsEvent Hidden;
		Hidden.Type = EOpenMobileAdsEventType::Hidden;
		Sink->Submit(MoveTemp(Hidden));
		ScheduleCacheExpirationCheck();
		Sink->Commit();
		return FOpenMobileAdsOperationResult::Accepted(HideRequestId);
	}

	if (!Provider->Hide(Request, Sink, Error))
	{
		Sink->Invalidate();
		ActiveRequests.Remove(HideRequestId);
		*Status = PreviousStatus;
		Error = OpenMobileAdsPrivate::NormalizeProviderError(
			MoveTemp(Error),
			EOpenMobileAdsFailureStage::Hide,
			Placement,
			Provider->GetProviderName(),
			TEXT("The ads provider rejected the hide request without a typed error.")
		);
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}

	TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>
		SupersededShowContext;
	if (
		PreviousStatus.ActiveRequestId.IsValid()
		&& ActiveRequests.RemoveAndCopyValue(
			PreviousStatus.ActiveRequestId,
			SupersededShowContext
		)
		&& SupersededShowContext
	)
	{
		ForgetShowRewardContext(PreviousStatus.ActiveRequestId);
		CancelRetrySchedule(*SupersededShowContext);
		SupersededShowContext->EventSink->Invalidate();
	}
	CancelAutomaticPreload(Placement);
	ScheduleCacheExpirationCheck();
	Sink->Commit();
	return FOpenMobileAdsOperationResult::Accepted(HideRequestId);
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::DestroyAd(FName Placement)
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				Placement,
				EOpenMobileAdsFailureStage::Teardown
			)
		);
	}
	EnsureRuntime();
	if (bDeinitialized || !EventDispatcher)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Teardown,
			Placement,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}

	IOpenMobileAdsProvider* Provider = nullptr;
	FOpenMobileAdsResolvedPlacement ResolvedPlacement;
	FOpenMobileAdsError Error = ValidatePlacementForProvider(
		Placement,
		Provider,
		ResolvedPlacement
	);
	if (Error.IsSet())
	{
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}

	const FOpenMobileAdsProviderCapabilities ProviderCapabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* FormatCapabilities =
		ProviderCapabilities.FindFormat(ResolvedPlacement.Format);
	if (!FormatCapabilities || !FormatCapabilities->bCanDestroy)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeUnsupportedFormatError(
				Placement,
				Provider->GetProviderName(),
				EOpenMobileAdsFailureStage::Teardown
			)
		);
	}

	FOpenMobileAdsPlacementStatus* ExistingStatus = PlacementStatuses.Find(Placement);
	if (ExistingStatus && ExistingStatus->State == EOpenMobileAdPlacementState::Destroying)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Busy,
			EOpenMobileAdsFailureStage::Teardown,
			Placement,
			TEXT("The placement already has a destroy operation in progress."),
			Provider->GetProviderName()
		));
	}

	const bool bHadStatus = ExistingStatus != nullptr;
	const FOpenMobileAdsPlacementStatus PreviousStatus = bHadStatus
		? *ExistingStatus
		: FOpenMobileAdsPlacementStatus();
	const FGuid SupersededRequestId = bHadStatus
		? PreviousStatus.ActiveRequestId
		: FGuid();
	FOpenMobileAdsPlacementStatus& Status = PlacementStatuses.FindOrAdd(Placement);
	Status.Placement = Placement;
	Status.Format = ResolvedPlacement.Format;
	Status.Provider = Provider->GetProviderName();
	Status.State = EOpenMobileAdPlacementState::Destroying;
	Status.ActiveRequestId = FGuid::NewGuid();

	FOpenMobileAdsDestroyRequest Request;
	Request.RequestId = Status.ActiveRequestId;
	Request.Placement = Placement;
	const TSharedRef<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe>(
			EventDispatcher.ToSharedRef(),
			Status.Provider,
			Placement,
			Status.Format,
			EOpenMobileAdsFailureStage::Teardown,
			Status.ActiveRequestId,
			Status.CachedAdId
		);
	TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		MakeShared<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>();
	Context->Placement = Placement;
	Context->Provider = Status.Provider;
	Context->Format = Status.Format;
	Context->Stage = EOpenMobileAdsFailureStage::Teardown;
	Context->EventSink = Sink;
	ActiveRequests.Add(Status.ActiveRequestId, Context);
	if (!Provider->Destroy(Request, Sink, Error))
	{
		Sink->Invalidate();
		ActiveRequests.Remove(Status.ActiveRequestId);
		if (bHadStatus)
		{
			PlacementStatuses[Placement] = PreviousStatus;
		}
		else
		{
			PlacementStatuses.Remove(Placement);
		}
		Error = OpenMobileAdsPrivate::NormalizeProviderError(
			MoveTemp(Error),
			EOpenMobileAdsFailureStage::Teardown,
			Placement,
			Provider->GetProviderName(),
			TEXT("The ads provider rejected the destroy request without a typed error.")
		);
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}
	if (SupersededRequestId != Status.ActiveRequestId)
	{
		CancelSupersededRequest(SupersededRequestId);
	}
	CancelAutomaticPreload(Placement);
	ScheduleCacheExpirationCheck();
	Sink->Commit();
	return FOpenMobileAdsOperationResult::Accepted(Status.ActiveRequestId);
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::DestroyAllAds()
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				NAME_None,
				EOpenMobileAdsFailureStage::Teardown
			)
		);
	}
	EnsureRuntime();
	if (bDeinitialized || !EventDispatcher)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Teardown,
			NAME_None,
			TEXT("The ads subsystem has been deinitialized.")
		));
	}
	if (ServiceState != EOpenMobileAdsServiceState::Ready)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeServiceNotReadyError(
				NAME_None,
				ServiceState,
				InitializationError
			)
		);
	}

	FOpenMobileAdsError Error;
	IOpenMobileAdsProvider* Provider = FindProvider(&Error);
	if (!Provider)
	{
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}
	for (const TPair<FGuid, TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>>& Pair : ActiveRequests)
	{
		if (Pair.Value && Pair.Value->Stage == EOpenMobileAdsFailureStage::Teardown)
		{
			return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Busy,
				EOpenMobileAdsFailureStage::Teardown,
				NAME_None,
				TEXT("Another destroy operation is already in progress."),
				Provider->GetProviderName()
			));
		}
	}

	TArray<FGuid> SupersededRequestIds;
	SupersededRequestIds.Reserve(ActiveRequests.Num());
	for (const TPair<FGuid, TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>>& Pair : ActiveRequests)
	{
		SupersededRequestIds.Add(Pair.Key);
	}

	FOpenMobileAdsDestroyRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.bAllPlacements = true;
	const TSharedRef<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe>(
			EventDispatcher.ToSharedRef(),
			Provider->GetProviderName(),
			NAME_None,
			EOpenMobileAdFormat::Rewarded,
			EOpenMobileAdsFailureStage::Teardown,
			Request.RequestId
		);
	TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		MakeShared<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>();
	Context->Provider = Provider->GetProviderName();
	Context->Stage = EOpenMobileAdsFailureStage::Teardown;
	Context->EventSink = Sink;
	Context->bRestoreStatusesOnFailure = false;
	ActiveRequests.Add(Request.RequestId, Context);
	if (!Provider->Destroy(Request, Sink, Error))
	{
		Sink->Invalidate();
		ActiveRequests.Remove(Request.RequestId);
		Error = OpenMobileAdsPrivate::NormalizeProviderError(
			MoveTemp(Error),
			EOpenMobileAdsFailureStage::Teardown,
			NAME_None,
			Provider->GetProviderName(),
			TEXT("The ads provider rejected the service-wide destroy request without a typed error.")
		);
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}
	for (FGuid SupersededRequestId : SupersededRequestIds)
	{
		CancelSupersededRequest(SupersededRequestId);
	}
	CancelAllAutomaticPreloads();
	for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		Pair.Value.State = EOpenMobileAdPlacementState::Destroying;
		Pair.Value.ActiveRequestId = Request.RequestId;
	}
	ScheduleCacheExpirationCheck();
	Sink->Commit();
	return FOpenMobileAdsOperationResult::Accepted(Request.RequestId);
}

void UOpenMobileAdsSubsystem::CancelSupersededRequest(FGuid RequestId)
{
	TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context;
	if (
		!RequestId.IsValid()
		|| !ActiveRequests.RemoveAndCopyValue(RequestId, Context)
		|| !Context
	)
	{
		return;
	}
	ForgetShowRewardContext(RequestId);
	if (Context->Stage == EOpenMobileAdsFailureStage::Show && FullscreenLifecycle)
	{
		FullscreenLifecycle->End(
			EOpenMobileAdsFullscreenSurface::Ad,
			RequestId
		);
	}
	CancelRetrySchedule(*Context);
	Context->EventSink->Invalidate();
	if (Context->bProviderAttemptActive)
	{
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsPrivate::FindRegisteredProvider(Context->Provider))
		{
			Provider->Cancel(RequestId);
		}
	}
}

FOpenMobileAdsOperationResult UOpenMobileAdsSubsystem::CancelRequest(FGuid RequestId)
{
	if (!IsInGameThread())
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeOperationThreadError(
				NAME_None,
				EOpenMobileAdsFailureStage::Teardown
			)
		);
	}

	TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* FoundContext =
		ActiveRequests.Find(RequestId);
	if (!RequestId.IsValid() || !FoundContext || !FoundContext->IsValid())
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Teardown,
			NAME_None,
			TEXT("The ads request is not active and cannot be cancelled.")
		));
	}

	const TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		FoundContext->ToSharedRef();
	ForgetShowRewardContext(RequestId);
	if (Context->Stage == EOpenMobileAdsFailureStage::Show && FullscreenLifecycle)
	{
		FullscreenLifecycle->End(
			EOpenMobileAdsFullscreenSurface::Ad,
			RequestId
		);
	}
	CancelRetrySchedule(*Context);
	Context->EventSink->Invalidate();
	if (Context->bProviderAttemptActive)
	{
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsPrivate::FindRegisteredProvider(Context->Provider))
		{
			Provider->Cancel(RequestId);
		}
	}

	TArray<FName> StatusesToRemove;
	for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		if (Pair.Value.ActiveRequestId != RequestId)
		{
			continue;
		}
		if (Context->Stage == EOpenMobileAdsFailureStage::Teardown)
		{
			ReleaseCachedAd(Pair.Value);
			Pair.Value.State = EOpenMobileAdPlacementState::Idle;
			Pair.Value.ActiveRequestId.Invalidate();
			Pair.Value.LastError = FOpenMobileAdsError();
		}
		else if (
			Context->Stage == EOpenMobileAdsFailureStage::Show
			|| Context->Stage == EOpenMobileAdsFailureStage::Hide
		)
		{
			ReleaseCachedAd(Pair.Value);
			Pair.Value.State = EOpenMobileAdPlacementState::Idle;
			Pair.Value.LastError = FOpenMobileAdsError();
		}
		else if (const FOpenMobileAdsPlacementStatus* Previous =
			Context->PreviousStatuses.Find(Pair.Key))
		{
			Pair.Value = *Previous;
		}
		else
		{
			StatusesToRemove.Add(Pair.Key);
		}
	}
	for (FName Placement : StatusesToRemove)
	{
		PlacementStatuses.Remove(Placement);
	}
	ActiveRequests.Remove(RequestId);
	ScheduleCacheExpirationCheck();

	FOpenMobileAdsEvent Cancelled;
	Cancelled.Type = EOpenMobileAdsEventType::Failed;
	Cancelled.Placement = Context->Placement;
	Cancelled.Format = Context->Format;
	Cancelled.Provider = Context->Provider;
	Cancelled.RequestId = RequestId;
	if (const FOpenMobileAdsPlacementStatus* Status =
		PlacementStatuses.Find(Context->Placement))
	{
		Cancelled.PlacementState = Status->State;
		Cancelled.CachedAdId = Status->CachedAdId;
	}
	Cancelled.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::Cancelled,
		Context->Stage,
		Context->Placement,
		TEXT("The ads request was cancelled."),
		Context->Provider
	);
	CancelledRequestEvents.Add(RequestId);
	SubmitServiceEvent(MoveTemp(Cancelled));
	return FOpenMobileAdsOperationResult::Accepted(RequestId);
}

bool UOpenMobileAdsSubsystem::IsReady(FName Placement) const
{
	const FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement);
	return Status
		&& OpenMobileAdsPrivate::HasReusableCachedAdState(Status->State)
		&& Status->CachedAdId.IsValid()
		&& !IsCachedAdExpired(*Status);
}

FOpenMobileAdsCanShowResult UOpenMobileAdsSubsystem::CanShow(FName Placement) const
{
	return EvaluateCanShow(Placement, nullptr, nullptr);
}

FOpenMobileAdsCanShowResult UOpenMobileAdsSubsystem::EvaluateCanShow(
	FName Placement,
	IOpenMobileAdsProvider* KnownProvider,
	const FOpenMobileAdsResolvedPlacement* KnownPlacement
) const
{
	FOpenMobileAdsCanShowPolicyContext Context;
	const FOpenMobileAdsPlacementSettings* Configuration = FindConfiguredPlacement(Placement);
	if (!Configuration)
	{
		return FOpenMobileAdsCanShowPolicy::Evaluate(Context);
	}

	Context.bPlacementConfigured = true;
	const FOpenMobileAdsResolvedPlacement Resolved = KnownPlacement
		? *KnownPlacement
		: Configuration->Resolve(OpenMobileAdsGetCurrentPlatform());
	Context.bPlacementEnabled = Resolved.bEnabled;
	Context.ServiceState = ServiceState;
	if (ServiceState != EOpenMobileAdsServiceState::Ready)
	{
		Context.ServiceExplanation = OpenMobileAdsPrivate::MakeServiceNotReadyError(
			Placement,
			ServiceState,
			InitializationError
		).Explanation;
		return FOpenMobileAdsCanShowPolicy::Evaluate(Context);
	}

	FOpenMobileAdsError ProviderError;
	IOpenMobileAdsProvider* Provider = KnownProvider;
	if (!Provider)
	{
		Provider = FindProvider(&ProviderError);
	}
	Context.bProviderAvailable = Provider != nullptr;
	Context.ProviderExplanation = ProviderError.Explanation;
	if (!Provider)
	{
		return FOpenMobileAdsCanShowPolicy::Evaluate(Context);
	}
	const FOpenMobileAdsProviderCapabilities ProviderCapabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* FormatCapabilities =
		ProviderCapabilities.FindFormat(Resolved.Format);
	Context.bFormatSupported = FormatCapabilities && FormatCapabilities->bCanShow;
	Context.bPrivacyAllowed = EvaluateCanRequestAds(Provider).bCanRequestAds;

	const FDateTime Now = GetCacheUtcNow();
	const FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement);
	if (Status)
	{
		Context.PlacementState = Status->State;
		Context.bHasCachedAd = Status->CachedAdId.IsValid()
			&& Status->Format == Resolved.Format
			&& Status->Provider == Provider->GetProviderName();
		Context.bExpired = IsCachedAdExpired(*Status);
	}
	if (FrequencyCapTracker)
	{
		const FOpenMobileAdsFrequencyCapDecision CapDecision =
			FrequencyCapTracker->Evaluate(
				Placement,
				Resolved.FrequencyCap,
				Now,
				GetCacheMonotonicSeconds()
			);
		Context.bFrequencyCapped = CapDecision.IsCapped();
		Context.FrequencyCapScope = CapDecision.Scope;
		Context.FrequencyCapEndsAt = CapDecision.NextEligibleAt;
	}
	if (CooldownTracker)
	{
		const FOpenMobileAdsCooldownDecision CooldownDecision =
			CooldownTracker->Evaluate(
				Placement,
				Resolved.Format,
				Resolved.CooldownSeconds,
				GetDefault<UOpenMobileAdsSettings>()
					->CooldownPolicy.FullscreenCooldownSeconds,
				Now,
				GetCacheMonotonicSeconds()
			);
		Context.bCooldownActive = CooldownDecision.IsActive();
		Context.CooldownEndsAt = CooldownDecision.NextEligibleAt;
	}
	Context.bOffline = bPlatformDefinitelyOffline.Load();
	Context.bLifecycleConflict = !bApplicationActive || !bApplicationInForeground;
	if (OpenMobileAdsPrivate::UsesFullscreenLifecycle(Resolved.Format))
	{
		Context.bLifecycleConflict = Context.bLifecycleConflict
			|| (FullscreenLifecycle && FullscreenLifecycle->IsOccupied());
		for (const TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
		{
			if (
				Pair.Key != Placement
				&& Pair.Value.State == EOpenMobileAdPlacementState::Showing
				&& OpenMobileAdsPrivate::UsesFullscreenLifecycle(Pair.Value.Format)
			)
			{
				Context.bLifecycleConflict = true;
				break;
			}
		}
	}

	return FOpenMobileAdsCanShowPolicy::Evaluate(Context);
}

FOpenMobileAdsPlacementStatus UOpenMobileAdsSubsystem::GetPlacementStatus(
	FName Placement
) const
{
	if (const FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement))
	{
		return *Status;
	}

	FOpenMobileAdsPlacementStatus Result;
	Result.Placement = Placement;
	const FOpenMobileAdsPlacementSettings* Configuration = FindConfiguredPlacement(Placement);
	if (!Configuration)
	{
		Result.State = EOpenMobileAdPlacementState::Failed;
		Result.LastError = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::UnknownPlacement,
			EOpenMobileAdsFailureStage::Configuration,
			Placement,
			TEXT("The requested ads placement is not configured.")
		);
		return Result;
	}

	const FOpenMobileAdsResolvedPlacement Resolved =
		Configuration->Resolve(OpenMobileAdsGetCurrentPlatform());
	Result.Format = Resolved.Format;
	Result.State = Resolved.bEnabled
		? EOpenMobileAdPlacementState::Idle
		: EOpenMobileAdPlacementState::Disabled;
	if (IOpenMobileAdsProvider* Provider = FindProvider())
	{
		Result.Provider = Provider->GetProviderName();
	}
	return Result;
}

FOpenMobileAdsProviderCapabilities UOpenMobileAdsSubsystem::GetProviderCapabilities() const
{
	if (IOpenMobileAdsProvider* Provider = FindProvider())
	{
		return Provider->GetCapabilities();
	}
	return FOpenMobileAdsProviderCapabilities();
}

void UOpenMobileAdsSubsystem::SubmitServiceEvent(FOpenMobileAdsEvent Event)
{
	if (EventDispatcher)
	{
		EventDispatcher->Submit(MoveTemp(Event));
	}
}

void UOpenMobileAdsSubsystem::CancelRetrySchedule(
	FOpenMobileAdsActiveRequestContext& Context
)
{
	if (RetryScheduler)
	{
		RetryScheduler->Cancel(Context.RetryScheduleHandle);
	}
	else
	{
		Context.RetryScheduleHandle.Reset();
	}
	Context.PendingRetryDelaySeconds = 0.0;
	Context.bRetryPending = false;
	Context.bWaitingForConnectivity = false;
}

bool UOpenMobileAdsSubsystem::TryScheduleLoadRetry(
	const FOpenMobileAdsEvent& Event
)
{
	if (
		Event.Type != EOpenMobileAdsEventType::LoadFailed
		|| bDeinitialized
		|| ServiceState != EOpenMobileAdsServiceState::Ready
		|| FOpenMobileAdsErrorClassifier::Classify(Event.Error)
			!= EOpenMobileAdsRetryClassification::Retryable
	)
	{
		return false;
	}

	TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* FoundContext =
		ActiveRequests.Find(Event.RequestId);
	if (
		!FoundContext
		|| !FoundContext->IsValid()
		|| (*FoundContext)->Stage != EOpenMobileAdsFailureStage::Load
		|| (*FoundContext)->Provider != Event.Provider
		|| (*FoundContext)->Placement != Event.Placement
	)
	{
		return false;
	}

	const TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		FoundContext->ToSharedRef();
	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	const FOpenMobileAdsRetryPolicy& Policy =
		Settings->GetRetryPolicyForError(Event.Error.Code);
	if (
		!Policy.IsValid()
		|| !RetryRandomSource
		|| !RetryScheduler
		|| Context->RetryAttempts >= Policy.ResolveMaxRetryAttempts(
			Context->PlacementMaxRetryAttempts
		)
	)
	{
		return false;
	}

	IOpenMobileAdsProvider* Provider =
		OpenMobileAdsPrivate::FindRegisteredProvider(Context->Provider);
	if (
		!Provider
		|| Context->Provider != SelectedProviderName
		|| !EvaluateCanRequestAds(Provider).bCanRequestAds
	)
	{
		return false;
	}

	Context->EventSink->Invalidate();
	Context->bProviderAttemptActive = false;
	Context->bRetryPending = true;
	Context->PendingRetryDelaySeconds =
		FOpenMobileAdsRetryDelayCalculator::Calculate(
			Policy,
			Context->RetryAttempts + 1,
			*RetryRandomSource
		);
	if (bPlatformDefinitelyOffline.Load())
	{
		Context->bWaitingForConnectivity = true;
		return true;
	}

	const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
	Context->RetryScheduleHandle = RetryScheduler->Schedule(
		Context->PendingRetryDelaySeconds,
		[WeakThis, RequestId = Event.RequestId]()
		{
			if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->StartPendingLoadRetry(RequestId);
			}
		}
	);
	if (Context->RetryScheduleHandle.IsValid())
	{
		return true;
	}
	Context->bRetryPending = false;
	return false;
}

void UOpenMobileAdsSubsystem::StartPendingLoadRetry(FGuid RequestId)
{
	check(IsInGameThread());
	TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* FoundContext =
		ActiveRequests.Find(RequestId);
	if (
		bDeinitialized
		|| !FoundContext
		|| !FoundContext->IsValid()
		|| !(*FoundContext)->bRetryPending
	)
	{
		return;
	}

	const TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		FoundContext->ToSharedRef();
	Context->RetryScheduleHandle.Reset();
	if (bPlatformDefinitelyOffline.Load())
	{
		Context->PendingRetryDelaySeconds = 0.0;
		Context->bWaitingForConnectivity = true;
		return;
	}

	if (
		ServiceState != EOpenMobileAdsServiceState::Ready
		|| Context->Provider != SelectedProviderName
	)
	{
		SubmitPendingLoadFailure(
			RequestId,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Load,
				Context->Placement,
				TEXT("The selected ads provider changed before the retry began."),
				Context->Provider,
				TEXT("Start a new load after the ads service is ready.")
			)
		);
		return;
	}

	IOpenMobileAdsProvider* Provider =
		OpenMobileAdsPrivate::FindRegisteredProvider(Context->Provider);
	if (!Provider)
	{
		SubmitPendingLoadFailure(
			RequestId,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Load,
				Context->Placement,
				TEXT("The ads provider is unavailable for the scheduled retry."),
				Context->Provider,
				TEXT("Keep the selected provider enabled until the load finishes.")
			)
		);
		return;
	}

	const FOpenMobileAdsCanRequestAdsResult RequestDecision =
		EvaluateCanRequestAds(Provider);
	if (!RequestDecision.bCanRequestAds)
	{
		SubmitPendingLoadFailure(
			RequestId,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::PrivacyBlocked,
				EOpenMobileAdsFailureStage::Consent,
				Context->Placement,
				RequestDecision.Explanation,
				Context->Provider,
				TEXT("Start a new load after consent allows ad requests.")
			)
		);
		return;
	}

	FOpenMobileAdsLoadRequest Request = Context->LoadRequest;
	Request.PrivacyContext =
		OpenMobileAdsPrivate::MakeProviderPrivacyContext(PrivacySnapshot);
	const TSharedRef<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe>(
			EventDispatcher.ToSharedRef(),
			Context->Provider,
			Context->Placement,
			Context->Format,
			EOpenMobileAdsFailureStage::Load,
			RequestId
		);
	Context->EventSink = Sink;
	Context->PendingRetryDelaySeconds = 0.0;
	Context->bRetryPending = false;
	Context->bWaitingForConnectivity = false;
	Context->bProviderAttemptActive = true;
	++Context->RetryAttempts;

	FOpenMobileAdsError Error;
	if (!Provider->Load(Request, Sink, Error))
	{
		Sink->Invalidate();
		Context->bProviderAttemptActive = false;
		Error = OpenMobileAdsPrivate::NormalizeProviderError(
			MoveTemp(Error),
			EOpenMobileAdsFailureStage::Load,
			Context->Placement,
			Context->Provider,
			TEXT("The ads provider rejected the retry without a typed error.")
		);
		SubmitPendingLoadFailure(RequestId, MoveTemp(Error));
		return;
	}
	Sink->Commit();
}

void UOpenMobileAdsSubsystem::ResumeConnectivityDeferredRetries()
{
	check(IsInGameThread());
	if (bDeinitialized || bPlatformDefinitelyOffline.Load() || !RetryScheduler)
	{
		return;
	}

	for (const TPair<FGuid, TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>>& Pair : ActiveRequests)
	{
		if (
			!Pair.Value
			|| !Pair.Value->bRetryPending
			|| !Pair.Value->bWaitingForConnectivity
		)
		{
			continue;
		}
		Pair.Value->bWaitingForConnectivity = false;
		const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
		Pair.Value->RetryScheduleHandle = RetryScheduler->Schedule(
			Pair.Value->PendingRetryDelaySeconds,
			[WeakThis, RequestId = Pair.Key]()
			{
				if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
				{
					Subsystem->StartPendingLoadRetry(RequestId);
				}
			}
		);
	}
}

void UOpenMobileAdsSubsystem::StopPrivacyBlockedRetries()
{
	check(IsInGameThread());
	if (bDeinitialized || ServiceState != EOpenMobileAdsServiceState::Ready)
	{
		return;
	}

	TArray<FGuid> BlockedRequests;
	for (const TPair<FGuid, TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>>& Pair : ActiveRequests)
	{
		if (
			!Pair.Value
			|| Pair.Value->Stage != EOpenMobileAdsFailureStage::Load
			|| !Pair.Value->bRetryPending
		)
		{
			continue;
		}
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsPrivate::FindRegisteredProvider(Pair.Value->Provider))
		{
			if (!EvaluateCanRequestAds(Provider).bCanRequestAds)
			{
				BlockedRequests.Add(Pair.Key);
			}
		}
	}

	for (FGuid RequestId : BlockedRequests)
	{
		const TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* Context =
			ActiveRequests.Find(RequestId);
		if (!Context || !Context->IsValid())
		{
			continue;
		}
		const FOpenMobileAdsCanRequestAdsResult Decision =
			EvaluateCanRequestAds(
				OpenMobileAdsPrivate::FindRegisteredProvider((*Context)->Provider)
			);
		SubmitPendingLoadFailure(
			RequestId,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::PrivacyBlocked,
				EOpenMobileAdsFailureStage::Consent,
				(*Context)->Placement,
				Decision.Explanation,
				(*Context)->Provider,
				TEXT("Start a new load after consent allows ad requests.")
			)
		);
	}
}

void UOpenMobileAdsSubsystem::RequestConfiguredAutomaticPreloads()
{
	check(IsInGameThread());
	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	if (
		bDeinitialized
		|| !Settings->PreloadPolicy.bEnabled
		|| !Settings->PreloadPolicy.IsValid()
	)
	{
		CancelAllAutomaticPreloads();
		return;
	}

	for (const FOpenMobileAdsPlacementSettings& Placement : Settings->Placements)
	{
		const FOpenMobileAdsResolvedPlacement Resolved =
			Placement.Resolve(OpenMobileAdsGetCurrentPlatform());
		if (Resolved.bEnabled && Resolved.bPreload)
		{
			RequestAutomaticPreload(
				Resolved.Placement,
				Settings->PreloadPolicy.TriggerDelaySeconds
			);
		}
	}
}

void UOpenMobileAdsSubsystem::RequestAutomaticPreload(
	FName Placement,
	double MinimumDelaySeconds
)
{
	check(IsInGameThread());
	if (bDeinitialized || Placement.IsNone() || !RetryScheduler)
	{
		return;
	}

	TSharedPtr<FOpenMobileAdsAutomaticPreloadContext>& Context =
		AutomaticPreloads.FindOrAdd(Placement);
	if (!Context)
	{
		Context = MakeShared<FOpenMobileAdsAutomaticPreloadContext>();
	}
	Context->EarliestStartMonotonicSeconds = FMath::Max(
		Context->EarliestStartMonotonicSeconds,
		GetCacheMonotonicSeconds() + FMath::Max(0.0, MinimumDelaySeconds)
	);
	ScheduleAutomaticPreload(Placement);
}

bool UOpenMobileAdsSubsystem::ResolveAutomaticPreloadDelay(
	FName Placement,
	double& OutDelaySeconds,
	bool& bOutCancel
) const
{
	OutDelaySeconds = 0.0;
	bOutCancel = false;
	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	const FOpenMobileAdsPlacementSettings* Configuration =
		Settings->FindPlacement(Placement);
	if (
		bDeinitialized
		|| !Settings->PreloadPolicy.bEnabled
		|| !Settings->PreloadPolicy.IsValid()
		|| !Configuration
	)
	{
		bOutCancel = true;
		return false;
	}

	const FOpenMobileAdsResolvedPlacement Resolved =
		Configuration->Resolve(OpenMobileAdsGetCurrentPlatform());
	if (!Resolved.bEnabled || !Resolved.bPreload)
	{
		bOutCancel = true;
		return false;
	}
	if (const FOpenMobileAdsPlacementStatus* Status =
		PlacementStatuses.Find(Placement))
	{
		if (
			Status->State == EOpenMobileAdPlacementState::Loading
			|| (
				OpenMobileAdsPrivate::HasReusableCachedAdState(Status->State)
				&& Status->CachedAdId.IsValid()
			)
			|| Status->State == EOpenMobileAdPlacementState::Hiding
			|| Status->State == EOpenMobileAdPlacementState::Destroying
		)
		{
			bOutCancel = true;
			return false;
		}
		if (Status->State == EOpenMobileAdPlacementState::Showing)
		{
			return false;
		}
	}
	if (
		ServiceState != EOpenMobileAdsServiceState::Ready
		|| !bApplicationActive
		|| !bApplicationInForeground
		|| bPlatformDefinitelyOffline.Load()
	)
	{
		return false;
	}

	IOpenMobileAdsProvider* Provider = FindProvider();
	if (!Provider || Provider->GetProviderName() != SelectedProviderName)
	{
		return false;
	}
	const FOpenMobileAdsProviderCapabilities ProviderCapabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* FormatCapabilities =
		ProviderCapabilities.FindFormat(Resolved.Format);
	if (
		!FormatCapabilities
		|| !FormatCapabilities->bCanLoad
		|| !FormatCapabilities->bSupportsPreload
	)
	{
		bOutCancel = true;
		return false;
	}
	if (!EvaluateCanRequestAds(Provider).bCanRequestAds)
	{
		return false;
	}
	const FDateTime NowUtc = GetCacheUtcNow();
	FDateTime PacingEndsAt;
	if (FrequencyCapTracker)
	{
		const FOpenMobileAdsFrequencyCapDecision CapDecision =
			FrequencyCapTracker->Evaluate(
				Placement,
				Resolved.FrequencyCap,
				NowUtc,
				GetCacheMonotonicSeconds()
			);
		if (CapDecision.Scope == EOpenMobileAdsFrequencyCapScope::Session)
		{
			bOutCancel = true;
			return false;
		}
		PacingEndsAt = CapDecision.NextEligibleAt;
	}
	if (CooldownTracker)
	{
		const FOpenMobileAdsCooldownDecision CooldownDecision =
			CooldownTracker->Evaluate(
				Placement,
				Resolved.Format,
				Resolved.CooldownSeconds,
				GetDefault<UOpenMobileAdsSettings>()
					->CooldownPolicy.FullscreenCooldownSeconds,
				NowUtc,
				GetCacheMonotonicSeconds()
			);
		PacingEndsAt = FMath::Max(
			PacingEndsAt,
			CooldownDecision.NextEligibleAt
		);
	}

	const TSharedPtr<FOpenMobileAdsAutomaticPreloadContext>* Context =
		AutomaticPreloads.Find(Placement);
	if (!Context || !Context->IsValid())
	{
		bOutCancel = true;
		return false;
	}
	OutDelaySeconds = FMath::Max(
		0.0,
		(*Context)->EarliestStartMonotonicSeconds
			- GetCacheMonotonicSeconds()
	);
	if (PacingEndsAt > NowUtc)
	{
		OutDelaySeconds = FMath::Max(
			OutDelaySeconds,
			(PacingEndsAt - NowUtc).GetTotalSeconds()
		);
	}
	return true;
}

void UOpenMobileAdsSubsystem::ScheduleAutomaticPreload(FName Placement)
{
	check(IsInGameThread());
	TSharedPtr<FOpenMobileAdsAutomaticPreloadContext>* FoundContext =
		AutomaticPreloads.Find(Placement);
	if (
		!FoundContext
		|| !FoundContext->IsValid()
		|| (*FoundContext)->ScheduleHandle.IsValid()
		|| !RetryScheduler
	)
	{
		return;
	}

	double DelaySeconds = 0.0;
	bool bCancel = false;
	if (!ResolveAutomaticPreloadDelay(Placement, DelaySeconds, bCancel))
	{
		if (bCancel)
		{
			CancelAutomaticPreload(Placement);
		}
		return;
	}

	const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
	(*FoundContext)->ScheduleHandle = RetryScheduler->Schedule(
		DelaySeconds,
		[WeakThis, Placement]()
		{
			if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->StartAutomaticPreload(Placement);
			}
		}
	);
}

void UOpenMobileAdsSubsystem::StartAutomaticPreload(FName Placement)
{
	check(IsInGameThread());
	TSharedPtr<FOpenMobileAdsAutomaticPreloadContext>* FoundContext =
		AutomaticPreloads.Find(Placement);
	if (!FoundContext || !FoundContext->IsValid())
	{
		return;
	}
	(*FoundContext)->ScheduleHandle.Reset();

	double DelaySeconds = 0.0;
	bool bCancel = false;
	if (!ResolveAutomaticPreloadDelay(Placement, DelaySeconds, bCancel))
	{
		if (bCancel)
		{
			CancelAutomaticPreload(Placement);
		}
		return;
	}
	if (DelaySeconds > 0.0)
	{
		ScheduleAutomaticPreload(Placement);
		return;
	}

	AutomaticPreloads.Remove(Placement);
	const FOpenMobileAdsOperationResult Result = LoadAd(Placement);
	if (
		!Result.bAccepted
		&& FOpenMobileAdsErrorClassifier::Classify(Result.Error)
			!= EOpenMobileAdsRetryClassification::Terminal
	)
	{
		RequestAutomaticPreload(
			Placement,
			GetDefault<UOpenMobileAdsSettings>()
				->PreloadPolicy.RecoverableFailureDelaySeconds
		);
	}
}

void UOpenMobileAdsSubsystem::PauseAutomaticPreloads()
{
	check(IsInGameThread());
	for (TPair<FName, TSharedPtr<FOpenMobileAdsAutomaticPreloadContext>>& Pair :
		AutomaticPreloads)
	{
		if (Pair.Value && RetryScheduler)
		{
			RetryScheduler->Cancel(Pair.Value->ScheduleHandle);
		}
	}
}

void UOpenMobileAdsSubsystem::ReevaluateAutomaticPreloads()
{
	check(IsInGameThread());
	PauseAutomaticPreloads();
	TArray<FName> Placements;
	AutomaticPreloads.GetKeys(Placements);
	for (const FName Placement : Placements)
	{
		ScheduleAutomaticPreload(Placement);
	}
}

void UOpenMobileAdsSubsystem::CancelAutomaticPreload(FName Placement)
{
	check(IsInGameThread());
	TSharedPtr<FOpenMobileAdsAutomaticPreloadContext> Context;
	if (!AutomaticPreloads.RemoveAndCopyValue(Placement, Context) || !Context)
	{
		return;
	}
	if (RetryScheduler)
	{
		RetryScheduler->Cancel(Context->ScheduleHandle);
	}
}

void UOpenMobileAdsSubsystem::CancelAllAutomaticPreloads()
{
	check(IsInGameThread());
	PauseAutomaticPreloads();
	AutomaticPreloads.Reset();
}

void UOpenMobileAdsSubsystem::SubmitPendingLoadFailure(
	FGuid RequestId,
	FOpenMobileAdsError Error
)
{
	TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* FoundContext =
		ActiveRequests.Find(RequestId);
	if (!FoundContext || !FoundContext->IsValid())
	{
		return;
	}
	const TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
		FoundContext->ToSharedRef();
	CancelRetrySchedule(*Context);
	Context->bProviderAttemptActive = false;

	FOpenMobileAdsEvent Failed;
	Failed.Type = EOpenMobileAdsEventType::LoadFailed;
	Failed.Placement = Context->Placement;
	Failed.Format = Context->Format;
	Failed.Provider = Context->Provider;
	Failed.RequestId = RequestId;
	Failed.Error = MoveTemp(Error);
	SubmitServiceEvent(MoveTemp(Failed));
}

void UOpenMobileAdsSubsystem::ReleaseCachedAd(FOpenMobileAdsPlacementStatus& Status)
{
	if (Status.CachedAdId.IsValid())
	{
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsPrivate::FindRegisteredProvider(Status.Provider))
		{
			Provider->ReleaseCachedAd(Status.CachedAdId);
		}
		ImpressedCachedAds.Remove(Status.CachedAdId);
		CacheExpirationMonotonicDeadlines.Remove(Status.CachedAdId);
		Status.CachedAdId.Invalidate();
	}
	Status.CachedAt = FDateTime();
	Status.ExpiresAt = FDateTime();
}

void UOpenMobileAdsSubsystem::RememberDismissedShow(
	FGuid RequestId,
	FGuid CachedAdId
)
{
	if (
		!RequestId.IsValid()
		|| !CachedAdId.IsValid()
		|| DismissedShowCachedAds.Contains(RequestId)
	)
	{
		return;
	}

	DismissedShowCachedAds.Add(RequestId, CachedAdId);
	DismissedShowRequestOrder.Add(RequestId);
	while (
		DismissedShowRequestOrder.Num()
		> OpenMobileAdsPrivate::MaxDismissedShowRewardContexts
	)
	{
		const FGuid ExpiredRequestId = DismissedShowRequestOrder[0];
		DismissedShowRequestOrder.RemoveAt(0, 1, EAllowShrinking::No);
		DismissedShowCachedAds.Remove(ExpiredRequestId);
		RewardedShowRequests.Remove(ExpiredRequestId);
	}
}

bool UOpenMobileAdsSubsystem::IsRememberedDismissedShow(
	FGuid RequestId,
	FGuid CachedAdId
) const
{
	const FGuid* RememberedCachedAdId = DismissedShowCachedAds.Find(RequestId);
	return RememberedCachedAdId && *RememberedCachedAdId == CachedAdId;
}

void UOpenMobileAdsSubsystem::ForgetShowRewardContext(FGuid RequestId)
{
	RewardedShowRequests.Remove(RequestId);
	if (DismissedShowCachedAds.Remove(RequestId) > 0)
	{
		DismissedShowRequestOrder.RemoveSingle(RequestId);
	}
}

FDateTime UOpenMobileAdsSubsystem::GetCacheUtcNow() const
{
	return CacheClock ? CacheClock->UtcNow() : FDateTime::UtcNow();
}

double UOpenMobileAdsSubsystem::GetCacheMonotonicSeconds() const
{
	return CacheClock
		? CacheClock->MonotonicSeconds()
		: FPlatformTime::Seconds();
}

bool UOpenMobileAdsSubsystem::IsCachedAdExpired(
	const FOpenMobileAdsPlacementStatus& Status
) const
{
	if (Status.ExpiresAt == FDateTime())
	{
		return false;
	}
	if (Status.ExpiresAt <= GetCacheUtcNow())
	{
		return true;
	}
	const double* MonotonicDeadline =
		CacheExpirationMonotonicDeadlines.Find(Status.CachedAdId);
	return MonotonicDeadline
		&& GetCacheMonotonicSeconds() >= *MonotonicDeadline;
}

void UOpenMobileAdsSubsystem::ExpireCachedAds()
{
	check(IsInGameThread());
	if (bDeinitialized)
	{
		return;
	}
	TArray<FOpenMobileAdsEvent> ExpiredEvents;
	TArray<FName> ExpiredPreloadPlacements;
	for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		FOpenMobileAdsPlacementStatus& Status = Pair.Value;
		if (
			!OpenMobileAdsPrivate::HasReusableCachedAdState(Status.State)
			|| !Status.CachedAdId.IsValid()
			|| !IsCachedAdExpired(Status)
		)
		{
			continue;
		}

		FOpenMobileAdsEvent& Expired = ExpiredEvents.Emplace_GetRef();
		Expired.Type = EOpenMobileAdsEventType::Expired;
		Expired.Placement = Status.Placement;
		Expired.Format = Status.Format;
		Expired.PlacementState = EOpenMobileAdPlacementState::Idle;
		Expired.Provider = Status.Provider;
		Expired.RequestId = Status.ActiveRequestId;
		Expired.CachedAdId = Status.CachedAdId;
		Expired.CacheExpiresAt = Status.ExpiresAt;
		PendingExpiredCachedAdEvents.Add(Status.CachedAdId);
		const FOpenMobileAdsPlacementSettings* Configuration =
			FindConfiguredPlacement(Status.Placement);
		if (
			Configuration
			&& Configuration->Resolve(OpenMobileAdsGetCurrentPlatform()).bPreload
		)
		{
			ExpiredPreloadPlacements.Add(Status.Placement);
		}
		ReleaseCachedAd(Status);
		Status.State = EOpenMobileAdPlacementState::Idle;
		Status.ActiveRequestId.Invalidate();
		Status.LastError = FOpenMobileAdsError();
	}

	for (FOpenMobileAdsEvent& Expired : ExpiredEvents)
	{
		SubmitServiceEvent(MoveTemp(Expired));
	}
	for (const FName Placement : ExpiredPreloadPlacements)
	{
		RequestAutomaticPreload(
			Placement,
			GetDefault<UOpenMobileAdsSettings>()->PreloadPolicy.TriggerDelaySeconds
		);
	}
	ScheduleCacheExpirationCheck();
}

void UOpenMobileAdsSubsystem::ScheduleCacheExpirationCheck()
{
	if (CacheExpirationTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CacheExpirationTickerHandle);
		CacheExpirationTickerHandle.Reset();
	}
	if (bDeinitialized)
	{
		return;
	}

	const FDateTime NowUtc = GetCacheUtcNow();
	const double NowMonotonic = GetCacheMonotonicSeconds();
	double EarliestDelaySeconds = TNumericLimits<double>::Max();
	for (const TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		const FOpenMobileAdsPlacementStatus& Status = Pair.Value;
		if (
			OpenMobileAdsPrivate::HasReusableCachedAdState(Status.State)
			&& Status.CachedAdId.IsValid()
			&& Status.ExpiresAt != FDateTime()
		)
		{
			double DelaySeconds = FMath::Max(
				0.0,
				(Status.ExpiresAt - NowUtc).GetTotalSeconds()
			);
			if (const double* MonotonicDeadline =
				CacheExpirationMonotonicDeadlines.Find(Status.CachedAdId))
			{
				DelaySeconds = FMath::Min(
					DelaySeconds,
					FMath::Max(0.0, *MonotonicDeadline - NowMonotonic)
				);
			}
			EarliestDelaySeconds = FMath::Min(
				EarliestDelaySeconds,
				DelaySeconds
			);
		}
	}
	if (EarliestDelaySeconds == TNumericLimits<double>::Max())
	{
		return;
	}

	CacheExpirationTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UOpenMobileAdsSubsystem::HandleCacheExpirationTick
		),
		static_cast<float>(FMath::Min(
			EarliestDelaySeconds,
			static_cast<double>(TNumericLimits<float>::Max())
		))
	);
}

bool UOpenMobileAdsSubsystem::HandleCacheExpirationTick(float DeltaTime)
{
	static_cast<void>(DeltaTime);
	CacheExpirationTickerHandle.Reset();
	ExpireCachedAds();
	return false;
}

void UOpenMobileAdsSubsystem::HandleProviderEvent(FOpenMobileAdsEvent Event)
{
	check(IsInGameThread());
	if (bDeinitialized)
	{
		return;
	}

	if (
		Event.Type == EOpenMobileAdsEventType::ProviderRegistered
		|| Event.Type == EOpenMobileAdsEventType::ProviderUnregistered
	)
	{
		OpenMobileAdsPrivate::LogEvent(Event);
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
		return;
	}

	if (
		Event.Type == EOpenMobileAdsEventType::Failed
		&& Event.Error.Code == EOpenMobileAdsErrorCode::Cancelled
		&& CancelledRequestEvents.Remove(Event.RequestId) > 0
	)
	{
		OpenMobileAdsPrivate::LogEvent(Event);
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
		HandleConvenienceRewardedEvent(Event);
		return;
	}

	if (Event.Type == EOpenMobileAdsEventType::Expired)
	{
		if (PendingExpiredCachedAdEvents.Remove(Event.CachedAdId) == 0)
		{
			return;
		}
		OpenMobileAdsPrivate::LogEvent(Event);
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
		return;
	}

	if (Event.Placement.IsNone() && Event.Type == EOpenMobileAdsEventType::Destroyed)
	{
		TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* FoundContext =
			ActiveRequests.Find(Event.RequestId);
		if (
			!FoundContext
			|| !FoundContext->IsValid()
			|| (*FoundContext)->Provider != Event.Provider
			|| !(*FoundContext)->Placement.IsNone()
		)
		{
			return;
		}
		(*FoundContext)->EventSink->Invalidate();
		ActiveRequests.Remove(Event.RequestId);
		for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
		{
			if (Pair.Value.ActiveRequestId == Event.RequestId)
			{
				ReleaseCachedAd(Pair.Value);
			}
		}
		PlacementStatuses.Reset();
		RewardedShowRequests.Reset();
		DismissedShowCachedAds.Reset();
		DismissedShowRequestOrder.Reset();
		ImpressedCachedAds.Reset();
		ScheduleCacheExpirationCheck();
		OpenMobileAdsPrivate::LogEvent(Event);
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
		return;
	}

	if (Event.Placement.IsNone() && Event.Type == EOpenMobileAdsEventType::Failed)
	{
		TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* FoundContext =
			ActiveRequests.Find(Event.RequestId);
		if (
			!FoundContext
			|| !FoundContext->IsValid()
			|| (*FoundContext)->Provider != Event.Provider
			|| !(*FoundContext)->Placement.IsNone()
		)
		{
			return;
		}

		const TSharedRef<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context =
			FoundContext->ToSharedRef();
		if (Context->bRestoreStatusesOnFailure)
		{
			TArray<FName> StatusesToRemove;
			for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
			{
				if (Pair.Value.ActiveRequestId != Event.RequestId)
				{
					continue;
				}
				if (const FOpenMobileAdsPlacementStatus* Previous =
					Context->PreviousStatuses.Find(Pair.Key))
				{
					Pair.Value = *Previous;
				}
				else
				{
					StatusesToRemove.Add(Pair.Key);
				}
			}
			for (FName Placement : StatusesToRemove)
			{
				PlacementStatuses.Remove(Placement);
			}
		}
		else
		{
			for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
			{
				if (Pair.Value.ActiveRequestId != Event.RequestId)
				{
					continue;
				}
				ReleaseCachedAd(Pair.Value);
				Pair.Value.State = EOpenMobileAdPlacementState::Failed;
				Pair.Value.LastError = Event.Error;
				Pair.Value.LastError.Placement = Pair.Key;
			}
		}
		Context->EventSink->Invalidate();
		ActiveRequests.Remove(Event.RequestId);
		ScheduleCacheExpirationCheck();
		OpenMobileAdsPrivate::LogEvent(Event);
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
		return;
	}

	FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Event.Placement);
	if (!Status || Status->Provider != Event.Provider)
	{
		return;
	}
	if (
		Event.Type == EOpenMobileAdsEventType::Loaded
		|| Event.Type == EOpenMobileAdsEventType::LoadFailed
	)
	{
		if (TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* Context =
			ActiveRequests.Find(Event.RequestId))
		{
			if (Context->IsValid())
			{
				(*Context)->bProviderAttemptActive = false;
			}
		}
		if (
			Event.Type == EOpenMobileAdsEventType::LoadFailed
			&& TryScheduleLoadRetry(Event)
		)
		{
			return;
		}
	}

	bool bBroadcast = false;
	bool bCacheScheduleChanged = false;
	switch (Event.Type)
	{
	case EOpenMobileAdsEventType::LoadStarted:
		bBroadcast = Status->State == EOpenMobileAdPlacementState::Loading
			&& Status->ActiveRequestId == Event.RequestId;
		break;

	case EOpenMobileAdsEventType::Loaded:
		if (
			Status->State == EOpenMobileAdPlacementState::Loading
			&& Status->ActiveRequestId == Event.RequestId
		)
		{
			if (!Event.CachedAdId.IsValid())
			{
				Event.Type = EOpenMobileAdsEventType::LoadFailed;
				Event.Error = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::ProviderFailure,
					EOpenMobileAdsFailureStage::Load,
					Event.Placement,
					TEXT("The ads provider reported a loaded ad without a cache identity."),
					Event.Provider,
					TEXT("Return a stable cache identity with every loaded event.")
				);
				const TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* Context =
					ActiveRequests.Find(Event.RequestId);
				const FOpenMobileAdsPlacementStatus* Previous = Context && Context->IsValid()
					? (*Context)->PreviousStatuses.Find(Event.Placement)
					: nullptr;
				if (
					Previous
					&& OpenMobileAdsPrivate::HasReusableCachedAdState(Previous->State)
				)
				{
					*Status = *Previous;
				}
				else
				{
					Status->State = EOpenMobileAdPlacementState::Failed;
				}
				Status->LastError = Event.Error;
			}
			else
			{
				ReleaseCachedAd(*Status);
				Status->State = EOpenMobileAdPlacementState::Ready;
				Status->CachedAdId = Event.CachedAdId;
				Status->CachedAt = Event.Timestamp;
				Status->ExpiresAt = Event.CacheExpiresAt;
				if (Status->ExpiresAt == FDateTime())
				{
					if (IOpenMobileAdsProvider* Provider =
						OpenMobileAdsPrivate::FindRegisteredProvider(Status->Provider))
					{
						const FOpenMobileAdsProviderCapabilities Capabilities =
							Provider->GetCapabilities();
						if (const FOpenMobileAdFormatCapabilities* Format =
							Capabilities.FindFormat(Status->Format))
						{
							if (
								FMath::IsFinite(Format->CacheLifetimeSeconds)
								&& Format->CacheLifetimeSeconds > 0.0
							)
							{
								Status->ExpiresAt = Status->CachedAt
									+ FTimespan::FromSeconds(Format->CacheLifetimeSeconds);
							}
						}
					}
				}
				Event.CacheExpiresAt = Status->ExpiresAt;
				if (Status->ExpiresAt != FDateTime())
				{
					const double RemainingLifetimeSeconds = FMath::Max(
						0.0,
						(Status->ExpiresAt - GetCacheUtcNow()).GetTotalSeconds()
					);
					CacheExpirationMonotonicDeadlines.Add(
						Status->CachedAdId,
						GetCacheMonotonicSeconds() + RemainingLifetimeSeconds
					);
				}
			}
			Event.PlacementState = Status->State;
			bBroadcast = true;
			bCacheScheduleChanged = true;
		}
		break;

	case EOpenMobileAdsEventType::LoadFailed:
		if (
			Status->State == EOpenMobileAdPlacementState::Loading
			&& Status->ActiveRequestId == Event.RequestId
		)
		{
			const TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>* Context =
				ActiveRequests.Find(Event.RequestId);
			const FOpenMobileAdsPlacementStatus* Previous = Context && Context->IsValid()
				? (*Context)->PreviousStatuses.Find(Event.Placement)
				: nullptr;
			if (
				Previous
				&& OpenMobileAdsPrivate::HasReusableCachedAdState(Previous->State)
			)
			{
				*Status = *Previous;
			}
			else
			{
				Status->State = EOpenMobileAdPlacementState::Failed;
			}
			Status->LastError = Event.Error;
			Event.PlacementState = Status->State;
			bBroadcast = true;
			bCacheScheduleChanged = true;
		}
		break;

	case EOpenMobileAdsEventType::ShowAccepted:
	case EOpenMobileAdsEventType::Shown:
		if (
			Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId
		)
		{
			Event.PlacementState = Status->State;
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::Impression:
		if (
			Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId
			&& !ImpressedCachedAds.Contains(Status->CachedAdId)
		)
		{
			ImpressedCachedAds.Add(Status->CachedAdId);
			RecordImpression(Event.Placement);
			Event.PlacementState = Status->State;
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::RewardEarned:
	{
		const bool bActiveShow =
			Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId
			&& Status->CachedAdId == Event.CachedAdId;
		const bool bDismissedShow = IsRememberedDismissedShow(
			Event.RequestId,
			Event.CachedAdId
		);
		if (
			(bActiveShow || bDismissedShow)
			&& !RewardedShowRequests.Contains(Event.RequestId)
		)
		{
			RewardedShowRequests.Add(Event.RequestId);
			Event.PlacementState = bActiveShow
				? EOpenMobileAdPlacementState::Showing
				: EOpenMobileAdPlacementState::Idle;
			bBroadcast = true;
		}
		break;
	}

	case EOpenMobileAdsEventType::Clicked:
		if (
			Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId
		)
		{
			Event.PlacementState = Status->State;
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::RevenuePaid:
		bBroadcast = Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId;
		break;

	case EOpenMobileAdsEventType::Dismissed:
		if (
			Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId
		)
		{
			if (FullscreenLifecycle)
			{
				FullscreenLifecycle->End(
					EOpenMobileAdsFullscreenSurface::Ad,
					Event.RequestId
				);
			}
			RememberDismissedShow(Event.RequestId, Status->CachedAdId);
			ReleaseCachedAd(*Status);
			Status->State = EOpenMobileAdPlacementState::Idle;
			Event.PlacementState = Status->State;
			bBroadcast = true;
			bCacheScheduleChanged = true;
		}
		break;

	case EOpenMobileAdsEventType::Hidden:
		if (
			Status->State == EOpenMobileAdPlacementState::Hiding
			&& Status->ActiveRequestId == Event.RequestId
		)
		{
			const TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>*
				Context = ActiveRequests.Find(Event.RequestId);
			const bool bPreserveCachedAd =
				Context
				&& Context->IsValid()
				&& (*Context)->bPreserveCachedAdOnHide;
			if (!bPreserveCachedAd)
			{
				ReleaseCachedAd(*Status);
			}
			Status->State = EOpenMobileAdPlacementState::Hidden;
			Status->LastError = FOpenMobileAdsError();
			Event.PlacementState = Status->State;
			bBroadcast = true;
			bCacheScheduleChanged = true;
		}
		break;

	case EOpenMobileAdsEventType::Destroyed:
		if (Status->ActiveRequestId == Event.RequestId)
		{
			ReleaseCachedAd(*Status);
			Status->State = EOpenMobileAdPlacementState::Idle;
			Event.PlacementState = Status->State;
			bBroadcast = true;
			bCacheScheduleChanged = true;
		}
		break;

	case EOpenMobileAdsEventType::Failed:
		if (Status->ActiveRequestId == Event.RequestId)
		{
			if (FullscreenLifecycle)
			{
				FullscreenLifecycle->End(
					EOpenMobileAdsFullscreenSurface::Ad,
					Event.RequestId
				);
			}
			ForgetShowRewardContext(Event.RequestId);
			ReleaseCachedAd(*Status);
			Status->State = EOpenMobileAdPlacementState::Failed;
			Status->LastError = Event.Error;
			Event.PlacementState = Status->State;
			bBroadcast = true;
			bCacheScheduleChanged = true;
		}
		break;

	case EOpenMobileAdsEventType::PlacementStateChanged:
	case EOpenMobileAdsEventType::Refreshed:
		bBroadcast = Status->ActiveRequestId == Event.RequestId;
		break;

	default:
		break;
	}

	if (bBroadcast)
	{
		if (bCacheScheduleChanged)
		{
			ScheduleCacheExpirationCheck();
		}
		const bool bTerminal = Event.Type == EOpenMobileAdsEventType::Loaded
			|| Event.Type == EOpenMobileAdsEventType::LoadFailed
			|| Event.Type == EOpenMobileAdsEventType::Dismissed
			|| Event.Type == EOpenMobileAdsEventType::Hidden
			|| Event.Type == EOpenMobileAdsEventType::Destroyed
			|| Event.Type == EOpenMobileAdsEventType::Failed;
		if (bTerminal)
		{
			TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context;
			if (ActiveRequests.RemoveAndCopyValue(Event.RequestId, Context) && Context)
			{
				CancelRetrySchedule(*Context);
				if (Event.Type != EOpenMobileAdsEventType::Dismissed)
				{
					Context->EventSink->Invalidate();
				}
			}
		}
		OpenMobileAdsPrivate::LogEvent(Event);
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
		HandleConvenienceRewardedEvent(Event);

		const UOpenMobileAdsSettings* Settings =
			GetDefault<UOpenMobileAdsSettings>();
		const bool bConsumed = Event.Type == EOpenMobileAdsEventType::Dismissed
			|| (
				Event.Type == EOpenMobileAdsEventType::Failed
				&& Event.Error.Stage == EOpenMobileAdsFailureStage::Show
				&& Event.Error.Code != EOpenMobileAdsErrorCode::Cancelled
			);
		const bool bRecoverableLoadFailure =
			Event.Type == EOpenMobileAdsEventType::LoadFailed
			&& FOpenMobileAdsErrorClassifier::Classify(Event.Error)
				!= EOpenMobileAdsRetryClassification::Terminal;
		if (bConsumed)
		{
			RequestAutomaticPreload(
				Event.Placement,
				Settings->PreloadPolicy.TriggerDelaySeconds
			);
			ReevaluateAutomaticPreloads();
		}
		else if (bRecoverableLoadFailure)
		{
			RequestAutomaticPreload(
				Event.Placement,
				Settings->PreloadPolicy.RecoverableFailureDelaySeconds
			);
		}
	}
}

void UOpenMobileAdsSubsystem::RecordImpression(FName Placement)
{
	FOpenMobileAdsFrequencyCap Policy;
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;
	bool bConfigured = false;
	if (const FOpenMobileAdsPlacementSettings* Configuration =
		FindConfiguredPlacement(Placement))
	{
		const FOpenMobileAdsResolvedPlacement Resolved =
			Configuration->Resolve(OpenMobileAdsGetCurrentPlatform());
		Policy = Resolved.FrequencyCap;
		Format = Resolved.Format;
		bConfigured = true;
	}
	if (FrequencyCapTracker)
	{
		const bool bPersisted = FrequencyCapTracker->RecordImpression(
			Placement,
			Policy,
			GetCacheUtcNow(),
			GetCacheMonotonicSeconds()
		);
		if (!bPersisted && !bFrequencyCapPersistenceWarningLogged)
		{
			bFrequencyCapPersistenceWarningLogged = true;
			FOpenMobileAdsLog::Write(
				EOpenMobileAdsLogLevel::Warning,
				TEXT("The rolling frequency-cap history could not be saved."),
				Placement
			);
		}
	}
	if (bConfigured && CooldownTracker)
	{
		CooldownTracker->RecordImpression(
			Placement,
			Format,
			GetCacheMonotonicSeconds()
		);
	}
}

void UOpenMobileAdsSubsystem::HandleProviderUnregistered(
	const FName& FeatureName,
	IModularFeature* Feature
)
{
	if (
		FeatureName != IOpenMobileAdsProvider::GetModularFeatureName()
		|| !Feature
	)
	{
		return;
	}

	const FName ProviderName =
		static_cast<IOpenMobileAdsProvider*>(Feature)->GetProviderName();
	if (!IsInGameThread())
	{
		const TWeakObjectPtr<UOpenMobileAdsSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, ProviderName]()
		{
			if (UOpenMobileAdsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->HandleProviderUnavailable(ProviderName);
			}
		});
		return;
	}
	HandleProviderUnavailable(ProviderName);
}

void UOpenMobileAdsSubsystem::HandleProviderUnavailable(FName ProviderName)
{
	check(IsInGameThread());
	if (bDeinitialized)
	{
		return;
	}
	if (ProviderName == SelectedProviderName)
	{
		CancelAllAutomaticPreloads();
	}
	if (
		ActiveConsentRequestId.IsValid()
		&& ActiveConsentAdsProviderName == ProviderName
	)
	{
		HandleConsentOperationFailed(
			ActiveConsentRequestId,
			ActiveConsentAdsProviderName,
			ActiveConsentProviderName,
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The consent provider was unregistered during an active operation."),
				ActiveConsentProviderName,
				TEXT("Keep the selected provider enabled until consent gathering finishes.")
			)
		);
	}
	bool bInitializationProviderUnavailable = false;
	if (
		ProviderName == SelectedProviderName
		&& (
			ServiceState == EOpenMobileAdsServiceState::Initializing
			|| ServiceState == EOpenMobileAdsServiceState::Ready
		)
	)
	{
		if (InitializationSink)
		{
			InitializationSink->Invalidate();
			InitializationSink.Reset();
		}
		InitializationError = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderUnavailable,
			EOpenMobileAdsFailureStage::Initialization,
			NAME_None,
			TEXT("The selected ads provider was unregistered after initialization began."),
			ProviderName,
			TEXT("Keep the selected provider enabled until the ads subsystem has shut down.")
		);
		ServiceState = EOpenMobileAdsServiceState::Failed;
		bProviderInitializationStarted = false;
		InitializationStatus.Error = InitializationError;
		InitializationStatus.LatencyMilliseconds = InitializationStartedSeconds > 0.0
			? (FPlatformTime::Seconds() - InitializationStartedSeconds) * 1000.0
			: InitializationStatus.LatencyMilliseconds;
		if (FOpenMobileAdsInitializationComponentStatus* Component =
			InitializationStatus.Components.FindByPredicate(
				[ProviderName](const FOpenMobileAdsInitializationComponentStatus& Candidate)
				{
					return Candidate.Type == EOpenMobileAdsInitializationComponentType::Provider
						&& Candidate.Name == ProviderName;
				}
			))
		{
			Component->State = EOpenMobileAdsInitializationState::Failed;
			Component->Error = InitializationError;
			if (Component->LatencyMilliseconds < 0.0)
			{
				Component->LatencyMilliseconds = InitializationStatus.LatencyMilliseconds;
			}
		}
		UpdatePartialInitializationState();
		bInitializationProviderUnavailable = true;
	}
	if (bInitializationProviderUnavailable)
	{
		BroadcastInitializationStatus();
	}
	TSet<FGuid> ServiceWideRequests;
	for (const TPair<FGuid, TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>>& Pair : ActiveRequests)
	{
		if (Pair.Value && Pair.Value->Provider == ProviderName)
		{
			if (
				Pair.Value->Stage == EOpenMobileAdsFailureStage::Show
				&& FullscreenLifecycle
			)
			{
				FullscreenLifecycle->End(
					EOpenMobileAdsFullscreenSurface::Ad,
					Pair.Key
				);
			}
			CancelRetrySchedule(*Pair.Value);
			Pair.Value->EventSink->Invalidate();
			Pair.Value->bProviderAttemptActive = false;
			if (Pair.Value->Placement.IsNone())
			{
				Pair.Value->bRestoreStatusesOnFailure = false;
				ServiceWideRequests.Add(Pair.Key);
			}
		}
	}
	for (FGuid RequestId : ServiceWideRequests)
	{
		FOpenMobileAdsEvent Failed;
		Failed.Type = EOpenMobileAdsEventType::Failed;
		Failed.Provider = ProviderName;
		Failed.RequestId = RequestId;
		Failed.Error = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderUnavailable,
			EOpenMobileAdsFailureStage::Teardown,
			NAME_None,
			TEXT("The ads provider was unregistered during a service-wide operation."),
			ProviderName,
			TEXT("Keep the provider enabled until the ads subsystem has shut down.")
		);
		SubmitServiceEvent(MoveTemp(Failed));
	}

	for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		FOpenMobileAdsPlacementStatus& Status = Pair.Value;
		if (
			Status.Provider != ProviderName
			|| Status.State == EOpenMobileAdPlacementState::Idle
			|| Status.State == EOpenMobileAdPlacementState::Disabled
		)
		{
			continue;
		}
		ImpressedCachedAds.Remove(Status.CachedAdId);
		CacheExpirationMonotonicDeadlines.Remove(Status.CachedAdId);
		Status.CachedAdId.Invalidate();
		Status.CachedAt = FDateTime();
		Status.ExpiresAt = FDateTime();
		if (ServiceWideRequests.Contains(Status.ActiveRequestId))
		{
			Status.State = EOpenMobileAdPlacementState::Failed;
			Status.LastError = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::Teardown,
				Status.Placement,
				TEXT("The ads provider was unregistered during a service-wide operation."),
				ProviderName,
				TEXT("Keep the provider enabled until the ads subsystem has shut down.")
			);
			continue;
		}
		Status.State = EOpenMobileAdPlacementState::Failed;
		Status.LastError = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderUnavailable,
			EOpenMobileAdsFailureStage::Teardown,
			Status.Placement,
			TEXT("The ads provider was unregistered during an active placement operation."),
			ProviderName,
			TEXT("Keep the provider enabled until the ads subsystem has shut down.")
		);

		FOpenMobileAdsEvent Failed;
		Failed.Type = EOpenMobileAdsEventType::Failed;
		Failed.Placement = Status.Placement;
		Failed.Format = Status.Format;
		Failed.Provider = ProviderName;
		Failed.RequestId = Status.ActiveRequestId;
		Failed.Error = Status.LastError;
		SubmitServiceEvent(MoveTemp(Failed));
	}
	RewardedShowRequests.Reset();
	DismissedShowCachedAds.Reset();
	DismissedShowRequestOrder.Reset();
	ScheduleCacheExpirationCheck();
}

void UOpenMobileAdsSubsystem::Deinitialize()
{
	if (ActiveConsentRequestId.IsValid())
	{
		if (IOpenMobileAdsProvider* ConsentProvider =
			OpenMobileAdsPrivate::FindRegisteredProvider(
				ActiveConsentAdsProviderName
			))
		{
			ConsentProvider->CancelConsent(ActiveConsentRequestId);
		}
		ClearConsentOperation(
			PrivacySnapshot.ConsentActivity
				== EOpenMobileAdsConsentActivity::PresentingForm
		);
	}
	bDeinitialized = true;
	ActiveTrackingAuthorizationRequestId.Invalidate();
	if (FullscreenLifecycle)
	{
		FullscreenLifecycle->Shutdown();
		FullscreenLifecycle.Reset();
	}
	ResetConvenienceRewardedOperation();
	CancelAllAutomaticPreloads();
	if (CacheExpirationTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CacheExpirationTickerHandle);
		CacheExpirationTickerHandle.Reset();
	}
	IOpenMobileAdsProvider* InitializationProvider = FindProvider();
	const bool bInitializationInProgress =
		ServiceState == EOpenMobileAdsServiceState::Initializing;
	ServiceState = EOpenMobileAdsServiceState::ShuttingDown;
	UpdatePartialInitializationState();
	BroadcastInitializationStatus();
	if (InitializationSink)
	{
		InitializationSink->Invalidate();
		InitializationSink.Reset();
	}
	if (
		InitializationProvider
		&& bInitializationInProgress
		&& InitializationRequestId.IsValid()
	)
	{
		InitializationProvider->Cancel(InitializationRequestId);
	}
	for (const TPair<FGuid, TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>>& Pair : ActiveRequests)
	{
		if (!Pair.Value)
		{
			continue;
		}
		CancelRetrySchedule(*Pair.Value);
		Pair.Value->EventSink->Invalidate();
		if (Pair.Value->bProviderAttemptActive)
		{
			if (IOpenMobileAdsProvider* Provider =
				OpenMobileAdsPrivate::FindRegisteredProvider(Pair.Value->Provider))
			{
				Provider->Cancel(Pair.Key);
			}
		}
	}
	ActiveRequests.Reset();
	RetryScheduler.Reset();
	RetryRandomSource.Reset();
	CancelledRequestEvents.Reset();
	for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		ReleaseCachedAd(Pair.Value);
	}
	if (bRuntimeInitialized)
	{
		IModularFeatures::Get().OnModularFeatureUnregistered().Remove(
			ProviderUnregisteredHandle
		);
		FCoreDelegates::OnNetworkConnectionChanged.Remove(
			NetworkConnectionChangedHandle
		);
		FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(
			ApplicationWillDeactivateHandle
		);
		FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(
			ApplicationHasReactivatedHandle
		);
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(
			ApplicationWillEnterBackgroundHandle
		);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(
			ApplicationHasEnteredForegroundHandle
		);
		bRuntimeInitialized = false;
	}
	if (EventDispatcher)
	{
		EventDispatcher->Invalidate();
		EventDispatcher.Reset();
	}
	PlacementStatuses.Reset();
	RewardedShowRequests.Reset();
	DismissedShowCachedAds.Reset();
	DismissedShowRequestOrder.Reset();
	ImpressedCachedAds.Reset();
	CacheExpirationMonotonicDeadlines.Reset();
	if (FrequencyCapTracker)
	{
		FrequencyCapTracker->Flush(
			GetCacheUtcNow(),
			GetCacheMonotonicSeconds()
		);
		FrequencyCapTracker.Reset();
	}
	if (CooldownTracker)
	{
		CooldownTracker->Reset();
		CooldownTracker.Reset();
	}
	PendingExpiredCachedAdEvents.Reset();
	CacheClock.Reset();
	if (InitializationProvider && bProviderInitializationStarted)
	{
		InitializationProvider->Shutdown();
	}
	SelectedProviderName = NAME_None;
	InitializationRequestId.Invalidate();
	InitializationError = FOpenMobileAdsError();
	InitializationStartedSeconds = 0.0;
	bProviderInitializationStarted = false;
	ConsentSignalDeliveryStatus =
		FOpenMobileAdsConsentSignalDeliverySnapshot();
	LastPropagatedConsentSignals = FOpenMobileAdsConsentSignals();
	bConsentSignalsPropagated = false;
	State = EOpenMobileRewardedAdState::Idle;
	Super::Deinitialize();
}

bool UOpenMobileAdsSubsystem::RequestAndShowRewardedAd()
{
	if (!IsInGameThread())
	{
		ReportAdFailure(FOpenMobileError::Make(
			EOpenMobileErrorCode::Internal,
			TEXT("Rewarded ads must be requested on the Unreal game thread.")
		));
		return false;
	}
	EnsureRuntime();
	if (ServiceState != EOpenMobileAdsServiceState::Ready)
	{
		const FOpenMobileAdsError Error = OpenMobileAdsPrivate::MakeServiceNotReadyError(
			NAME_None,
			ServiceState,
			InitializationError
		);
		ReportAdFailure(OpenMobileAdsPrivate::ToLegacyError(Error));
		return false;
	}
	if (State != EOpenMobileRewardedAdState::Idle)
	{
		ReportAdFailure(FOpenMobileError::Make(
			EOpenMobileErrorCode::Busy,
			TEXT("A rewarded ad is already loading or showing.")
		));
		return false;
	}

	FOpenMobileError PlacementError;
	const FName Placement = ResolveConvenienceRewardedPlacement(PlacementError);
	if (Placement.IsNone())
	{
		ReportAdFailure(MoveTemp(PlacementError));
		return false;
	}

	ActiveConvenienceRewardedPlacement = Placement;
	State = EOpenMobileRewardedAdState::Loading;
	if (IsReady(Placement))
	{
		OnAdLoaded.Broadcast();
		return StartConvenienceRewardedShow();
	}

	const FOpenMobileAdsOperationResult LoadResult = LoadAd(Placement);
	if (!LoadResult.bAccepted)
	{
		ResetConvenienceRewardedOperation();
		ReportAdFailure(OpenMobileAdsPrivate::ToLegacyError(LoadResult.Error));
		return false;
	}
	ConvenienceRewardedLoadRequestId = LoadResult.RequestId;
	return true;
}

bool UOpenMobileAdsSubsystem::IsSupported() const
{
	return FindProvider() != nullptr;
}

FName UOpenMobileAdsSubsystem::GetActiveProviderName() const
{
	const IOpenMobileAdsProvider* Provider = FindProvider();
	return Provider ? Provider->GetProviderName() : NAME_None;
}

FName UOpenMobileAdsSubsystem::ResolveConvenienceRewardedPlacement(
	FOpenMobileError& OutError
) const
{
	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	if (!Settings->ConvenienceRewardedPlacement.IsNone())
	{
		const FOpenMobileAdsPlacementSettings* Placement = Settings->FindPlacement(
			Settings->ConvenienceRewardedPlacement
		);
		if (
			Placement
			&& Placement->Format == EOpenMobileAdFormat::Rewarded
			&& Placement->Resolve(OpenMobileAdsGetCurrentPlatform()).bEnabled
		)
		{
			return Placement->Placement;
		}
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("The configured convenience rewarded placement is missing, disabled, or not rewarded.")
		);
		return NAME_None;
	}

	FName ResolvedPlacement;
	for (const FOpenMobileAdsPlacementSettings& Placement : Settings->Placements)
	{
		if (
			Placement.Format != EOpenMobileAdFormat::Rewarded
			|| !Placement.Resolve(OpenMobileAdsGetCurrentPlatform()).bEnabled
		)
		{
			continue;
		}
		if (!ResolvedPlacement.IsNone())
		{
			OutError = FOpenMobileError::Make(
				EOpenMobileErrorCode::NotConfigured,
				TEXT("More than one enabled rewarded placement exists. Set Convenience Rewarded Placement in OpenMobile Ads settings.")
			);
			return NAME_None;
		}
		ResolvedPlacement = Placement.Placement;
	}
	if (ResolvedPlacement.IsNone())
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("Configure one enabled rewarded placement before requesting a convenience rewarded ad.")
		);
	}
	return ResolvedPlacement;
}

bool UOpenMobileAdsSubsystem::StartConvenienceRewardedShow()
{
	const FOpenMobileAdsOperationResult ShowResult = ShowAd(
		ActiveConvenienceRewardedPlacement
	);
	if (!ShowResult.bAccepted)
	{
		ResetConvenienceRewardedOperation();
		ReportAdFailure(OpenMobileAdsPrivate::ToLegacyError(ShowResult.Error));
		return false;
	}
	ConvenienceRewardedShowRequestId = ShowResult.RequestId;
	return true;
}

void UOpenMobileAdsSubsystem::HandleConvenienceRewardedEvent(
	const FOpenMobileAdsEvent& Event
)
{
	if (Event.RequestId == ConvenienceRewardedLoadRequestId)
	{
		if (Event.Type == EOpenMobileAdsEventType::Loaded)
		{
			ConvenienceRewardedLoadRequestId.Invalidate();
			OnAdLoaded.Broadcast();
			StartConvenienceRewardedShow();
		}
		else if (
			Event.Type == EOpenMobileAdsEventType::LoadFailed
			|| Event.Type == EOpenMobileAdsEventType::Failed
		)
		{
			const FOpenMobileError Error = OpenMobileAdsPrivate::ToLegacyError(
				Event.Error
			);
			ResetConvenienceRewardedOperation();
			OnAdFailed.Broadcast(Error);
		}
		return;
	}
	if (Event.RequestId != ConvenienceRewardedShowRequestId)
	{
		return;
	}

	switch (Event.Type)
	{
	case EOpenMobileAdsEventType::Shown:
		State = EOpenMobileRewardedAdState::Showing;
		OnAdShown.Broadcast();
		break;
	case EOpenMobileAdsEventType::RewardEarned:
		if (Event.bHasReward)
		{
			const int64 Amount = FMath::Clamp<int64>(
				Event.Reward.Amount,
				1,
				MAX_int32
			);
			OnRewardEarned.Broadcast(
				static_cast<int32>(Amount),
				Event.Reward.Type
			);
		}
		break;
	case EOpenMobileAdsEventType::Dismissed:
		ResetConvenienceRewardedOperation();
		OnAdClosed.Broadcast();
		break;
	case EOpenMobileAdsEventType::Failed:
	{
		const FOpenMobileError Error = OpenMobileAdsPrivate::ToLegacyError(Event.Error);
		ResetConvenienceRewardedOperation();
		OnAdFailed.Broadcast(Error);
		break;
	}
	default:
		break;
	}
}

void UOpenMobileAdsSubsystem::ResetConvenienceRewardedOperation()
{
	ActiveConvenienceRewardedPlacement = NAME_None;
	ConvenienceRewardedLoadRequestId.Invalidate();
	ConvenienceRewardedShowRequestId.Invalidate();
	State = EOpenMobileRewardedAdState::Idle;
}

void UOpenMobileAdsSubsystem::ReportAdFailure(FOpenMobileError Error)
{
	FOpenMobileAdsLog::Write(
		EOpenMobileAdsLogLevel::Warning,
		Error.Message,
		NAME_None,
		FName(*Error.Provider)
	);
	OnAdFailed.Broadcast(Error);
}

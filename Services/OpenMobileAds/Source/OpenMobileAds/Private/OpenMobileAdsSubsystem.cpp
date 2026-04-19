#include "OpenMobileAdsSubsystem.h"

#include "Async/Async.h"
#include "Features/IModularFeatures.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileAdsCanShowPolicy.h"
#include "OpenMobileAdsDiagnostics.h"

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
			FGuid InCachedAdId = FGuid()
		)
			: Dispatcher(MoveTemp(InDispatcher))
			, Provider(InProvider)
			, Placement(InPlacement)
			, Format(InFormat)
			, OperationStage(InOperationStage)
			, RequestId(InRequestId)
			, CachedAdId(InCachedAdId)
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
			if (OperationStage != EOpenMobileAdsFailureStage::Show)
			{
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
			case EOpenMobileAdsEventType::RewardEarned:
			case EOpenMobileAdsEventType::RevenuePaid:
			case EOpenMobileAdsEventType::Refreshed:
				return true;

			case EOpenMobileAdsEventType::Dismissed:
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
			if (Event.Error.IsSet())
			{
				Event.Error.Provider = Provider;
				Event.Error.Placement = Placement;
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
		bool bCommitted = false;
		bool bShownSubmitted = false;
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
		case EOpenMobileAdsCanShowBlockReason::NotInitialized:
		case EOpenMobileAdsCanShowBlockReason::FrequencyCap:
		case EOpenMobileAdsCanShowBlockReason::Cooldown:
		case EOpenMobileAdsCanShowBlockReason::Offline:
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
				|| Code == EOpenMobileAdsErrorCode::InvalidState
		);
	}

	FOpenMobileAdsError NormalizeInitializationError(
		FOpenMobileAdsError Error,
		FName ProviderName,
		const TCHAR* FallbackExplanation
	)
	{
		if (!Error.IsSet())
		{
			Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderFailure,
				EOpenMobileAdsFailureStage::Initialization,
				NAME_None,
				FallbackExplanation,
				ProviderName
			);
		}
		if (Error.Stage == EOpenMobileAdsFailureStage::None)
		{
			Error.Stage = EOpenMobileAdsFailureStage::Initialization;
		}
		if (Error.Provider.IsNone())
		{
			Error.Provider = ProviderName;
		}
		return Error;
	}
}

struct FOpenMobileAdsActiveRequestContext
{
	FName Placement;
	FName Provider;
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;
	EOpenMobileAdsFailureStage Stage = EOpenMobileAdsFailureStage::Internal;
	TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink;
	TMap<FName, FOpenMobileAdsPlacementStatus> PreviousStatuses;
	bool bRestoreStatusesOnFailure = true;
};

namespace OpenMobileAdsPrivate
{
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
			Level = Event.Error.Code == EOpenMobileAdsErrorCode::Cancelled
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
			Settings->TestDeviceIdentifiers
		);
	Request.Privacy = Settings->Privacy;
	Request.RequestConfiguration = Settings->RequestConfiguration;

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

	if (Error.IsSet())
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
	if (Status.Error.IsSet())
	{
		Status.Error.Provider = ProviderName;
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
	NativeInitializationStatusChanged.Broadcast(InitializationStatus);
	OnInitializationStatusChanged.Broadcast(InitializationStatus);
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
		PrivacySnapshot.ChildDirectedTreatment = Privacy.ChildDirectedTreatment;
		PrivacySnapshot.UnderAgeOfConsent = Privacy.UnderAgeOfConsent;
		PrivacySnapshot.bCanRequestAds = !Privacy.bDelayProviderInitializationUntilConsent;
		PrivacySnapshot.Source = TEXT("ProjectSettings");
		PrivacySnapshot.LastUpdated = FDateTime::UtcNow();
		bPrivacySnapshotInitialized = true;
	}
	if (bRuntimeInitialized || bDeinitialized)
	{
		return;
	}

	EventDispatcher = MakeShared<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe>(*this);
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
}

void UOpenMobileAdsSubsystem::UpdatePrivacySnapshot(
	FOpenMobileAdsPrivacySnapshot Snapshot
)
{
	check(IsInGameThread());
	if (bDeinitialized)
	{
		return;
	}
	if (Snapshot.LastUpdated == FDateTime())
	{
		Snapshot.LastUpdated = FDateTime::UtcNow();
	}
	PrivacySnapshot = MoveTemp(Snapshot);
	bPrivacySnapshotInitialized = true;
}

void UOpenMobileAdsSubsystem::HandleNetworkConnectionChanged(
	ENetworkConnectionType ConnectionType
)
{
	bPlatformOffline.Store(
		ConnectionType == ENetworkConnectionType::None
		|| ConnectionType == ENetworkConnectionType::AirplaneMode
	);
}

void UOpenMobileAdsSubsystem::HandleApplicationWillDeactivate()
{
	bApplicationActive = false;
}

void UOpenMobileAdsSubsystem::HandleApplicationHasReactivated()
{
	bApplicationActive = true;
}

void UOpenMobileAdsSubsystem::HandleApplicationWillEnterBackground()
{
	bApplicationInForeground = false;
}

void UOpenMobileAdsSubsystem::HandleApplicationHasEnteredForeground()
{
	bApplicationInForeground = true;
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
	if (!PrivacySnapshot.bCanRequestAds)
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::PrivacyBlocked,
			EOpenMobileAdsFailureStage::Consent,
			Placement,
			TEXT("The current privacy state does not allow ad requests."),
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
		const bool bBusy = ExistingStatus->State == EOpenMobileAdPlacementState::Loading
			|| ExistingStatus->State == EOpenMobileAdPlacementState::Showing
			|| ExistingStatus->State == EOpenMobileAdPlacementState::Destroying;
		if (bBusy || (ExistingStatus->State == EOpenMobileAdPlacementState::Ready && !Options.bForceReload))
		{
			return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::Busy,
				EOpenMobileAdsFailureStage::Load,
				Placement,
				bBusy
					? TEXT("The placement already has an operation in progress.")
					: TEXT("The placement already has a ready ad."),
				Provider->GetProviderName()
			));
		}
	}

	const bool bHadStatus = ExistingStatus != nullptr;
	const FOpenMobileAdsPlacementStatus PreviousStatus = bHadStatus
		? *ExistingStatus
		: FOpenMobileAdsPlacementStatus();
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
	if (bHadStatus)
	{
		Context->PreviousStatuses.Add(Placement, PreviousStatus);
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
		if (!Error.IsSet())
		{
			Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderFailure,
				EOpenMobileAdsFailureStage::Load,
				Placement,
				TEXT("The ads provider rejected the load request without an error."),
				Provider->GetProviderName()
			);
		}
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}

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
	const TSharedRef<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe> Sink =
		MakeShared<OpenMobileAdsPrivate::FContextualEventSink, ESPMode::ThreadSafe>(
			EventDispatcher.ToSharedRef(),
			Status->Provider,
			Placement,
			Status->Format,
			EOpenMobileAdsFailureStage::Show,
			Status->ActiveRequestId,
			Status->CachedAdId
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
		Sink->Invalidate();
		ActiveRequests.Remove(Status->ActiveRequestId);
		*Status = PreviousStatus;
		if (!Error.IsSet())
		{
			Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderFailure,
				EOpenMobileAdsFailureStage::Show,
				Placement,
				TEXT("The ads provider rejected the show request without an error."),
				Provider->GetProviderName()
			);
		}
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
		if (!Error.IsSet())
		{
			Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderFailure,
				EOpenMobileAdsFailureStage::Teardown,
				Placement,
				TEXT("The ads provider rejected the destroy request without an error."),
				Provider->GetProviderName()
			);
		}
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}
	if (SupersededRequestId != Status.ActiveRequestId)
	{
		CancelSupersededRequest(SupersededRequestId);
	}
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
		if (!Error.IsSet())
		{
			Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderFailure,
				EOpenMobileAdsFailureStage::Teardown,
				NAME_None,
				TEXT("The ads provider rejected the service-wide destroy request without an error."),
				Provider->GetProviderName()
			);
		}
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
	}
	for (FGuid SupersededRequestId : SupersededRequestIds)
	{
		CancelSupersededRequest(SupersededRequestId);
	}
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
	Context->EventSink->Invalidate();
	if (IOpenMobileAdsProvider* Provider =
		OpenMobileAdsPrivate::FindRegisteredProvider(Context->Provider))
	{
		Provider->Cancel(RequestId);
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
	Context->EventSink->Invalidate();
	if (IOpenMobileAdsProvider* Provider =
		OpenMobileAdsPrivate::FindRegisteredProvider(Context->Provider))
	{
		Provider->Cancel(RequestId);
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
		else if (Context->Stage == EOpenMobileAdsFailureStage::Show)
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
		&& Status->State == EOpenMobileAdPlacementState::Ready
		&& Status->CachedAdId.IsValid()
		&& (
			Status->ExpiresAt == FDateTime()
			|| Status->ExpiresAt > FDateTime::UtcNow()
		);
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
	Context.bPrivacyAllowed = PrivacySnapshot.bCanRequestAds;

	const FDateTime Now = FDateTime::UtcNow();
	const FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement);
	if (Status)
	{
		Context.PlacementState = Status->State;
		Context.bHasCachedAd = Status->CachedAdId.IsValid()
			&& Status->Format == Resolved.Format
			&& Status->Provider == Provider->GetProviderName();
		Context.bExpired = Status->ExpiresAt != FDateTime()
			&& Status->ExpiresAt <= Now;
	}
	if (const TArray<FDateTime>* ImpressionTimestamps =
		ImpressionTimestampsByPlacement.Find(Placement))
	{
		if (
			Resolved.FrequencyCap.IsEnabled()
			&& FMath::IsFinite(Resolved.FrequencyCap.WindowSeconds)
			&& ImpressionTimestamps->Num() >= Resolved.FrequencyCap.MaxImpressions
		)
		{
			const int32 FirstCappedIndex =
				ImpressionTimestamps->Num() - Resolved.FrequencyCap.MaxImpressions;
			const FDateTime FrequencyCapEndsAt =
				(*ImpressionTimestamps)[FirstCappedIndex]
				+ FTimespan::FromSeconds(Resolved.FrequencyCap.WindowSeconds);
			if (FrequencyCapEndsAt > Now)
			{
				Context.bFrequencyCapped = true;
				Context.FrequencyCapEndsAt = FrequencyCapEndsAt;
			}
		}
		if (
			Resolved.CooldownSeconds > 0.0
			&& FMath::IsFinite(Resolved.CooldownSeconds)
			&& !ImpressionTimestamps->IsEmpty()
		)
		{
			const FDateTime CooldownEndsAt = ImpressionTimestamps->Last()
				+ FTimespan::FromSeconds(Resolved.CooldownSeconds);
			if (CooldownEndsAt > Now)
			{
				Context.bCooldownActive = true;
				Context.CooldownEndsAt = CooldownEndsAt;
			}
		}
	}
	Context.bOffline = bPlatformOffline.Load();
	Context.bLifecycleConflict = !bApplicationActive || !bApplicationInForeground;
	for (const TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		if (
			Pair.Key != Placement
			&& Pair.Value.State == EOpenMobileAdPlacementState::Showing
		)
		{
			Context.bLifecycleConflict = true;
			break;
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

void UOpenMobileAdsSubsystem::ReleaseCachedAd(FOpenMobileAdsPlacementStatus& Status)
{
	if (Status.CachedAdId.IsValid())
	{
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsPrivate::FindRegisteredProvider(Status.Provider))
		{
			Provider->ReleaseCachedAd(Status.CachedAdId);
		}
		RewardedCachedAds.Remove(Status.CachedAdId);
		ImpressedCachedAds.Remove(Status.CachedAdId);
		Status.CachedAdId.Invalidate();
	}
	Status.CachedAt = FDateTime();
	Status.ExpiresAt = FDateTime();
}

void UOpenMobileAdsSubsystem::ExpireCachedAds()
{
	check(IsInGameThread());
	const FDateTime Now = FDateTime::UtcNow();
	TArray<FOpenMobileAdsEvent> ExpiredEvents;
	for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		FOpenMobileAdsPlacementStatus& Status = Pair.Value;
		if (
			Status.State != EOpenMobileAdPlacementState::Ready
			|| !Status.CachedAdId.IsValid()
			|| Status.ExpiresAt == FDateTime()
			|| Status.ExpiresAt > Now
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
		PendingExpiredCachedAdEvents.Add(Status.CachedAdId);
		ReleaseCachedAd(Status);
		Status.State = EOpenMobileAdPlacementState::Idle;
		Status.ActiveRequestId.Invalidate();
		Status.LastError = FOpenMobileAdsError();
	}

	for (FOpenMobileAdsEvent& Expired : ExpiredEvents)
	{
		SubmitServiceEvent(MoveTemp(Expired));
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

	FDateTime EarliestExpiration;
	for (const TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		const FOpenMobileAdsPlacementStatus& Status = Pair.Value;
		if (
			Status.State == EOpenMobileAdPlacementState::Ready
			&& Status.CachedAdId.IsValid()
			&& Status.ExpiresAt != FDateTime()
			&& (EarliestExpiration == FDateTime() || Status.ExpiresAt < EarliestExpiration)
		)
		{
			EarliestExpiration = Status.ExpiresAt;
		}
	}
	if (EarliestExpiration == FDateTime())
	{
		return;
	}

	const double DelaySeconds = FMath::Max(
		0.0,
		(EarliestExpiration - FDateTime::UtcNow()).GetTotalSeconds()
	);
	CacheExpirationTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UOpenMobileAdsSubsystem::HandleCacheExpirationTick
		),
		static_cast<float>(DelaySeconds)
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
		RewardedCachedAds.Reset();
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
				if (Previous && Previous->State == EOpenMobileAdPlacementState::Ready)
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
				Status->ExpiresAt = FDateTime();
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
			if (Previous && Previous->State == EOpenMobileAdPlacementState::Ready)
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
			RecordImpression(Event.Placement, Event.Timestamp);
			Event.PlacementState = Status->State;
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::RewardEarned:
		if (
			Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId
			&& !RewardedCachedAds.Contains(Status->CachedAdId)
		)
		{
			RewardedCachedAds.Add(Status->CachedAdId);
			bBroadcast = true;
		}
		break;

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
			ReleaseCachedAd(*Status);
			Status->State = EOpenMobileAdPlacementState::Idle;
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
			|| Event.Type == EOpenMobileAdsEventType::Destroyed
			|| Event.Type == EOpenMobileAdsEventType::Failed;
		if (bTerminal)
		{
			TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe> Context;
			if (ActiveRequests.RemoveAndCopyValue(Event.RequestId, Context) && Context)
			{
				Context->EventSink->Invalidate();
			}
		}
		OpenMobileAdsPrivate::LogEvent(Event);
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
	}
}

void UOpenMobileAdsSubsystem::RecordImpression(
	FName Placement,
	FDateTime Timestamp
)
{
	if (Timestamp == FDateTime())
	{
		Timestamp = FDateTime::UtcNow();
	}

	int32 HistoryLimit = 1;
	if (const FOpenMobileAdsPlacementSettings* Configuration =
		FindConfiguredPlacement(Placement))
	{
		const FOpenMobileAdsResolvedPlacement Resolved =
			Configuration->Resolve(OpenMobileAdsGetCurrentPlatform());
		if (Resolved.FrequencyCap.IsEnabled())
		{
			HistoryLimit = Resolved.FrequencyCap.MaxImpressions;
		}
	}

	TArray<FDateTime>& ImpressionTimestamps =
		ImpressionTimestampsByPlacement.FindOrAdd(Placement);
	ImpressionTimestamps.Add(Timestamp);
	if (ImpressionTimestamps.Num() > HistoryLimit)
	{
		ImpressionTimestamps.RemoveAt(
			0,
			ImpressionTimestamps.Num() - HistoryLimit,
			EAllowShrinking::No
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
			Pair.Value->EventSink->Invalidate();
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
		RewardedCachedAds.Remove(Status.CachedAdId);
		ImpressedCachedAds.Remove(Status.CachedAdId);
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
	ScheduleCacheExpirationCheck();
}

void UOpenMobileAdsSubsystem::Deinitialize()
{
	bDeinitialized = true;
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
		Pair.Value->EventSink->Invalidate();
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsPrivate::FindRegisteredProvider(Pair.Value->Provider))
		{
			Provider->Cancel(Pair.Key);
		}
	}
	ActiveRequests.Reset();
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
	RewardedCachedAds.Reset();
	ImpressedCachedAds.Reset();
	ImpressionTimestampsByPlacement.Reset();
	PendingExpiredCachedAdEvents.Reset();
	if (InitializationProvider && bProviderInitializationStarted)
	{
		InitializationProvider->Shutdown();
	}
	SelectedProviderName = NAME_None;
	InitializationRequestId.Invalidate();
	InitializationError = FOpenMobileAdsError();
	InitializationStartedSeconds = 0.0;
	bProviderInitializationStarted = false;
	State = EOpenMobileRewardedAdState::Idle;
	Super::Deinitialize();
}

bool UOpenMobileAdsSubsystem::RequestAndShowRewardedAd()
{
	if (ServiceState != EOpenMobileAdsServiceState::Ready)
	{
		const FOpenMobileAdsError Error = OpenMobileAdsPrivate::MakeServiceNotReadyError(
			NAME_None,
			ServiceState,
			InitializationError
		);
		HandleAdFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			Error.Explanation
		));
		return false;
	}
	if (State != EOpenMobileRewardedAdState::Idle)
	{
		HandleAdFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Busy,
			TEXT("A rewarded ad is already loading or showing.")
		));
		return false;
	}

	FOpenMobileAdsError ProviderError;
	IOpenMobileAdsProvider* Provider = FindProvider(&ProviderError);
	if (!Provider)
	{
		HandleAdFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			ProviderError.Explanation
		));
		return false;
	}

	State = EOpenMobileRewardedAdState::Loading;
	FOpenMobileRewardedAdCallbacks Callbacks;
	Callbacks.OnLoaded = FOpenMobileRewardedAdLoadedCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleAdLoaded
	);
	Callbacks.OnShown = FOpenMobileRewardedAdShownCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleAdShown
	);
	Callbacks.OnEarned = FOpenMobileRewardedAdEarnedCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleRewardEarned
	);
	Callbacks.OnClosed = FOpenMobileRewardedAdClosedCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleAdClosed
	);
	Callbacks.OnFailed = FOpenMobileRewardedAdFailedCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleAdFailed
	);

	FOpenMobileError Error;
	if (!Provider->RequestAndShowRewardedAd(MoveTemp(Callbacks), Error))
	{
		if (!Error.IsSet())
		{
			Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Internal,
				TEXT("The ads provider rejected the request without returning an error."),
				FString(),
				Provider->GetProviderName().ToString()
			);
		}
		HandleAdFailed(MoveTemp(Error));
		return false;
	}

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

void UOpenMobileAdsSubsystem::HandleAdLoaded()
{
	if (State == EOpenMobileRewardedAdState::Loading)
	{
		OnAdLoaded.Broadcast();
	}
}

void UOpenMobileAdsSubsystem::HandleAdShown()
{
	State = EOpenMobileRewardedAdState::Showing;
	OnAdShown.Broadcast();
}

void UOpenMobileAdsSubsystem::HandleRewardEarned(
	int32 NetworkAmount,
	FString NetworkRewardType
)
{
	OnRewardEarned.Broadcast(NetworkAmount, NetworkRewardType);
}

void UOpenMobileAdsSubsystem::HandleAdClosed()
{
	State = EOpenMobileRewardedAdState::Idle;
	OnAdClosed.Broadcast();
}

void UOpenMobileAdsSubsystem::HandleAdFailed(FOpenMobileError Error)
{
	State = EOpenMobileRewardedAdState::Idle;
	FOpenMobileAdsLog::Write(
		EOpenMobileAdsLogLevel::Warning,
		Error.Message,
		NAME_None,
		FName(*Error.Provider)
	);
	OnAdFailed.Broadcast(Error);
}

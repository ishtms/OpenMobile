#include "OpenMobileAdsSubsystem.h"

#include "Async/Async.h"
#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileCoreLog.h"

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
	class FContextualEventSink final : public IOpenMobileAdsProviderEventSink
	{
	public:
		FContextualEventSink(
			TSharedRef<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe> InDispatcher,
			FName InProvider,
			FName InPlacement,
			EOpenMobileAdFormat InFormat,
			FGuid InRequestId,
			FGuid InCachedAdId = FGuid()
		)
			: Dispatcher(MoveTemp(InDispatcher))
			, Provider(InProvider)
			, Placement(InPlacement)
			, Format(InFormat)
			, RequestId(InRequestId)
			, CachedAdId(InCachedAdId)
		{
		}

		virtual void Submit(FOpenMobileAdsEvent Event) override
		{
			Normalize(Event);
			bool bForward = false;
			{
				FScopeLock Lock(&Mutex);
				if (!bValid)
				{
					return;
				}
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

		void Invalidate()
		{
			FScopeLock Lock(&Mutex);
			bValid = false;
			PendingEvents.Reset();
		}

	private:
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
		FGuid RequestId;
		FGuid CachedAdId;
		bool bCommitted = false;
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
}

void UOpenMobileAdsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bDeinitialized = false;
	EnsureRuntime();
}

void UOpenMobileAdsSubsystem::EnsureRuntime()
{
	if (bRuntimeInitialized || bDeinitialized)
	{
		return;
	}

	EventDispatcher = MakeShared<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe>(*this);
	ProviderUnregisteredHandle = IModularFeatures::Get().OnModularFeatureUnregistered().AddUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleProviderUnregistered
	);
	bRuntimeInitialized = true;
}

FName UOpenMobileAdsSubsystem::GetPreferredProviderName() const
{
	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	if (!Settings->PreferredProvider.IsNone())
	{
		return Settings->PreferredProvider;
	}

	FString LegacyPreferredProvider;
	GConfig->GetString(
		TEXT("OpenMobileAds"),
		TEXT("PreferredProvider"),
		LegacyPreferredProvider,
		GEngineIni
	);
	LegacyPreferredProvider.TrimStartAndEndInline();
	return LegacyPreferredProvider.IsEmpty()
		? NAME_None
		: FName(*LegacyPreferredProvider);
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
		FOpenMobileAdsProviderResolver::Resolve(Providers, GetPreferredProviderName());
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
			Status.ActiveRequestId
		);

	if (!Provider->Load(Request, Sink, Error))
	{
		Sink->Invalidate();
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
	if (!FormatCapabilities || !FormatCapabilities->bCanShow)
	{
		return FOpenMobileAdsOperationResult::Rejected(
			OpenMobileAdsPrivate::MakeUnsupportedFormatError(
				Placement,
				Provider->GetProviderName(),
				EOpenMobileAdsFailureStage::Show
			)
		);
	}

	FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement);
	if (!Status || Status->State != EOpenMobileAdPlacementState::Ready || !Status->CachedAdId.IsValid())
	{
		return FOpenMobileAdsOperationResult::Rejected(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::NotReady,
			EOpenMobileAdsFailureStage::Show,
			Placement,
			TEXT("The placement does not have a ready ad."),
			Provider->GetProviderName(),
			TEXT("Load the placement and wait for the loaded event before showing it.")
		));
	}

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
			Status->ActiveRequestId,
			Status->CachedAdId
		);

	if (!Provider->Show(Request, Sink, Error))
	{
		Sink->Invalidate();
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

	const bool bHadStatus = PlacementStatuses.Contains(Placement);
	const FOpenMobileAdsPlacementStatus PreviousStatus = bHadStatus
		? PlacementStatuses[Placement]
		: FOpenMobileAdsPlacementStatus();
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
			Status.ActiveRequestId,
			Status.CachedAdId
		);
	if (!Provider->Destroy(Request, Sink, Error))
	{
		Sink->Invalidate();
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

	FOpenMobileAdsError Error;
	IOpenMobileAdsProvider* Provider = FindProvider(&Error);
	if (!Provider)
	{
		return FOpenMobileAdsOperationResult::Rejected(MoveTemp(Error));
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
			Request.RequestId
		);
	if (!Provider->Destroy(Request, Sink, Error))
	{
		Sink->Invalidate();
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
	for (TPair<FName, FOpenMobileAdsPlacementStatus>& Pair : PlacementStatuses)
	{
		Pair.Value.State = EOpenMobileAdPlacementState::Destroying;
		Pair.Value.ActiveRequestId = Request.RequestId;
	}
	Sink->Commit();
	return FOpenMobileAdsOperationResult::Accepted(Request.RequestId);
}

bool UOpenMobileAdsSubsystem::IsReady(FName Placement) const
{
	const FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement);
	return Status
		&& Status->State == EOpenMobileAdPlacementState::Ready
		&& Status->CachedAdId.IsValid();
}

FOpenMobileAdsCanShowResult UOpenMobileAdsSubsystem::CanShow(FName Placement) const
{
	FOpenMobileAdsCanShowResult Result;
	const FOpenMobileAdsPlacementSettings* Configuration = FindConfiguredPlacement(Placement);
	if (!Configuration)
	{
		Result.BlockReason = EOpenMobileAdsCanShowBlockReason::UnknownPlacement;
		Result.Explanation = TEXT("The requested ads placement is not configured.");
		return Result;
	}

	const FOpenMobileAdsResolvedPlacement Resolved =
		Configuration->Resolve(OpenMobileAdsGetCurrentPlatform());
	if (!Resolved.bEnabled)
	{
		Result.BlockReason = EOpenMobileAdsCanShowBlockReason::Disabled;
		Result.Explanation = TEXT("The requested ads placement is disabled.");
		return Result;
	}

	FOpenMobileAdsError ProviderError;
	IOpenMobileAdsProvider* Provider = FindProvider(&ProviderError);
	if (!Provider)
	{
		Result.BlockReason = EOpenMobileAdsCanShowBlockReason::ProviderUnavailable;
		Result.Explanation = ProviderError.Explanation;
		return Result;
	}
	const FOpenMobileAdsProviderCapabilities ProviderCapabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* FormatCapabilities =
		ProviderCapabilities.FindFormat(Resolved.Format);
	if (!FormatCapabilities || !FormatCapabilities->bCanShow)
	{
		Result.BlockReason = EOpenMobileAdsCanShowBlockReason::UnsupportedFormat;
		Result.Explanation = TEXT("The selected provider cannot show this placement format.");
		return Result;
	}

	const FOpenMobileAdsPlacementStatus* Status = PlacementStatuses.Find(Placement);
	if (!Status)
	{
		Result.BlockReason = EOpenMobileAdsCanShowBlockReason::NotLoaded;
		Result.Explanation = TEXT("The placement has not loaded an ad.");
		return Result;
	}
	if (Status->State == EOpenMobileAdPlacementState::Loading)
	{
		Result.BlockReason = EOpenMobileAdsCanShowBlockReason::Loading;
		Result.Explanation = TEXT("The placement is still loading.");
		return Result;
	}
	if (Status->State != EOpenMobileAdPlacementState::Ready || !Status->CachedAdId.IsValid())
	{
		Result.BlockReason = EOpenMobileAdsCanShowBlockReason::NotLoaded;
		Result.Explanation = TEXT("The placement does not have a ready ad.");
		return Result;
	}

	Result.bCanShow = true;
	Result.BlockReason = EOpenMobileAdsCanShowBlockReason::None;
	return Result;
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
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
		return;
	}

	if (Event.Placement.IsNone() && Event.Type == EOpenMobileAdsEventType::Destroyed)
	{
		PlacementStatuses.Reset();
		RewardedCachedAds.Reset();
		ImpressedCachedAds.Reset();
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
			Status->State = EOpenMobileAdPlacementState::Ready;
			Status->CachedAdId = Event.CachedAdId.IsValid()
				? Event.CachedAdId
				: FGuid::NewGuid();
			Event.CachedAdId = Status->CachedAdId;
			Event.PlacementState = Status->State;
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::LoadFailed:
		if (
			Status->State == EOpenMobileAdPlacementState::Loading
			&& Status->ActiveRequestId == Event.RequestId
		)
		{
			Status->State = EOpenMobileAdPlacementState::Failed;
			Status->LastError = Event.Error;
			Event.PlacementState = Status->State;
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::ShowAccepted:
	case EOpenMobileAdsEventType::Shown:
		bBroadcast = Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId;
		break;

	case EOpenMobileAdsEventType::Impression:
		if (
			Status->ActiveRequestId == Event.RequestId
			&& !ImpressedCachedAds.Contains(Status->CachedAdId)
		)
		{
			ImpressedCachedAds.Add(Status->CachedAdId);
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::RewardEarned:
		if (
			Status->ActiveRequestId == Event.RequestId
			&& !RewardedCachedAds.Contains(Status->CachedAdId)
		)
		{
			RewardedCachedAds.Add(Status->CachedAdId);
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::Clicked:
	case EOpenMobileAdsEventType::RevenuePaid:
		bBroadcast = Status->ActiveRequestId == Event.RequestId;
		break;

	case EOpenMobileAdsEventType::Dismissed:
		if (
			Status->State == EOpenMobileAdPlacementState::Showing
			&& Status->ActiveRequestId == Event.RequestId
		)
		{
			Status->State = EOpenMobileAdPlacementState::Idle;
			Status->CachedAdId.Invalidate();
			Event.PlacementState = Status->State;
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::Destroyed:
		if (Status->ActiveRequestId == Event.RequestId)
		{
			Status->State = EOpenMobileAdPlacementState::Idle;
			Status->CachedAdId.Invalidate();
			Event.PlacementState = Status->State;
			bBroadcast = true;
		}
		break;

	case EOpenMobileAdsEventType::Failed:
		if (Status->ActiveRequestId == Event.RequestId)
		{
			Status->State = EOpenMobileAdPlacementState::Failed;
			Status->LastError = Event.Error;
			Event.PlacementState = Status->State;
			bBroadcast = true;
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
		NativeAdsEvent.Broadcast(Event);
		OnAdsEvent.Broadcast(Event);
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
}

void UOpenMobileAdsSubsystem::Deinitialize()
{
	bDeinitialized = true;
	if (bRuntimeInitialized)
	{
		IModularFeatures::Get().OnModularFeatureUnregistered().Remove(
			ProviderUnregisteredHandle
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
	State = EOpenMobileRewardedAdState::Idle;
	Super::Deinitialize();
}

bool UOpenMobileAdsSubsystem::RequestAndShowRewardedAd()
{
	if (State != EOpenMobileRewardedAdState::Idle)
	{
		HandleAdFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Busy,
			TEXT("A rewarded ad is already loading or showing.")
		));
		return false;
	}

	FOpenMobileAdsError SelectionError;
	IOpenMobileAdsProvider* Provider = FindProvider(&SelectionError);
	if (!Provider)
	{
		HandleAdFailed(FOpenMobileError::Make(
			SelectionError.Code == EOpenMobileAdsErrorCode::ProviderConflict
				? EOpenMobileErrorCode::NotConfigured
				: EOpenMobileErrorCode::NotSupported,
			SelectionError.Explanation
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
	UE_LOG(LogOpenMobile, Warning, TEXT("Ads request failed: %s"), *Error.Message);
	OnAdFailed.Broadcast(Error);
}

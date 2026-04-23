#include "OpenMobileAdsAdMobPlatform.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsAdMobBackend.h"
#include "IOpenMobileAdsProvider.h"
#include "OpenMobileAsync.h"
#include "OpenMobileAdsAdMobConsentMapper.h"

namespace OpenMobileAdsAdMobPlatformPrivate
{
	struct FRewardedLoadOperation
	{
		FGuid RequestId;
		FOnOpenMobileAdMobRewardedCached Loaded;
		FOnOpenMobileAdMobRewardedFailed Failed;
	};

	struct FRewardedShowOperation
	{
		FGuid RequestId;
		int64 LoadedRequestId = 0;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink;
		bool bRewardDispatched = false;
	};

	struct FConsentOperation
	{
		FGuid RequestId;
		FOnOpenMobileAdMobConsentCompleted Completed;
		FOnOpenMobileAdMobConsentFailed Failed;
	};

	TArray<FOnOpenMobileAdMobInitialized> InitializationDelegates;
	TArray<FOnOpenMobileAdMobInitializationStatus> InitializationStatusDelegates;
	TArray<FOpenMobileAdsInitializationComponentStatus> InitializationStatuses;
	FOnOpenMobileAdMobRewardedLoaded LoadedDelegate;
	FOnOpenMobileAdMobRewardedShown ShownDelegate;
	FOnOpenMobileAdMobRewardedEarned EarnedDelegate;
	FOnOpenMobileAdMobRewardedClosed ClosedDelegate;
	FOnOpenMobileAdMobRewardedFailed FailedDelegate;
	TMap<int64, FRewardedLoadOperation> RewardedLoadOperations;
	TMap<int64, FRewardedShowOperation> RewardedShowOperations;
	TMap<FGuid, int64> NativeLoadRequestIds;
	TMap<FGuid, int64> NativeShowRequestIds;
	TMap<FGuid, int64> LoadedRewardedAdRequestIds;
	TMap<int64, FConsentOperation> ConsentOperations;
	TMap<FGuid, int64> NativeConsentRequestIds;
	int64 NextRequestId = 0;
	int64 ActiveInitializationRequestId = 0;
	int64 ActiveRequestId = 0;
	bool bInitializationInProgress = false;
	bool bRequestInProgress = false;
	bool bRewardDispatched = false;
	bool bInitialized = false;

	void ResetInitialization()
	{
		InitializationDelegates.Reset();
		InitializationStatusDelegates.Reset();
		InitializationStatuses.Reset();
		ActiveInitializationRequestId = 0;
		bInitializationInProgress = false;
	}

	void BroadcastInitializationStatus(
		FOpenMobileAdsInitializationComponentStatus Status
	)
	{
		const int32 ExistingIndex = InitializationStatuses.IndexOfByPredicate(
			[&Status](const FOpenMobileAdsInitializationComponentStatus& Candidate)
			{
				return Candidate.Type == Status.Type
					&& Candidate.Name == Status.Name
					&& Candidate.Parent == Status.Parent;
			}
		);
		const int32 StatusIndex = ExistingIndex == INDEX_NONE
			? InitializationStatuses.Add(MoveTemp(Status))
			: ExistingIndex;
		if (ExistingIndex != INDEX_NONE)
		{
			InitializationStatuses[ExistingIndex] = MoveTemp(Status);
		}

		const FOpenMobileAdsInitializationComponentStatus& CurrentStatus =
			InitializationStatuses[StatusIndex];
		for (FOnOpenMobileAdMobInitializationStatus& Delegate : InitializationStatusDelegates)
		{
			Delegate.ExecuteIfBound(CurrentStatus);
		}
	}

	void ResetRequest()
	{
		LoadedDelegate.Unbind();
		ShownDelegate.Unbind();
		EarnedDelegate.Unbind();
		ClosedDelegate.Unbind();
		FailedDelegate.Unbind();
		ActiveRequestId = 0;
		bRequestInProgress = false;
		bRewardDispatched = false;
	}

	void ResetOperations()
	{
		RewardedLoadOperations.Reset();
		RewardedShowOperations.Reset();
		NativeLoadRequestIds.Reset();
		NativeShowRequestIds.Reset();
		LoadedRewardedAdRequestIds.Reset();
		ConsentOperations.Reset();
		NativeConsentRequestIds.Reset();
	}

	bool RemoveLoadOperation(
		int64 NativeRequestId,
		FRewardedLoadOperation& OutOperation
	)
	{
		if (!RewardedLoadOperations.RemoveAndCopyValue(NativeRequestId, OutOperation))
		{
			return false;
		}
		NativeLoadRequestIds.Remove(OutOperation.RequestId);
		return true;
	}

	bool RemoveShowOperation(
		int64 NativeRequestId,
		FRewardedShowOperation& OutOperation
	)
	{
		if (!RewardedShowOperations.RemoveAndCopyValue(NativeRequestId, OutOperation))
		{
			return false;
		}
		NativeShowRequestIds.Remove(OutOperation.RequestId);
		return true;
	}

	bool RemoveConsentOperation(
		int64 NativeRequestId,
		FConsentOperation& OutOperation
	)
	{
		if (!ConsentOperations.RemoveAndCopyValue(
			NativeRequestId,
			OutOperation
		))
		{
			return false;
		}
		NativeConsentRequestIds.Remove(OutOperation.RequestId);
		return true;
	}

	EOpenMobileAdsAdMobUMPConsentStatus ToConsentStatus(int32 Status)
	{
		switch (Status)
		{
		case 1:
			return EOpenMobileAdsAdMobUMPConsentStatus::NotRequired;
		case 2:
			return EOpenMobileAdsAdMobUMPConsentStatus::Required;
		case 3:
			return EOpenMobileAdsAdMobUMPConsentStatus::Obtained;
		default:
			return EOpenMobileAdsAdMobUMPConsentStatus::Unknown;
		}
	}

	EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement ToPrivacyOptions(
		int32 Requirement
	)
	{
		switch (Requirement)
		{
		case 1:
			return EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::NotRequired;
		case 2:
			return EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Required;
		default:
			return EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Unknown;
		}
	}

	FOpenMobileAdsConsentStatusUpdate MakeConsentUpdate(
		int32 ConsentStatus,
		bool bCanRequestAds,
		int32 PrivacyOptionsRequirement
	)
	{
		return FOpenMobileAdsAdMobConsentMapper::MapGdprState(
			ToConsentStatus(ConsentStatus),
			bCanRequestAds,
			TEXT("GoogleUMP"),
			ToPrivacyOptions(PrivacyOptionsRequirement)
		);
	}

	FOpenMobileAdsError MakeConsentError(
		const FString& ErrorCode,
		const FString& ErrorMessage
	)
	{
		FOpenMobileAdsErrorMappingContext Context;
		Context.Domain = EOpenMobileAdsErrorDomain::Consent;
		Context.Stage = EOpenMobileAdsFailureStage::Consent;
		Context.Provider = TEXT("GoogleUMP");
		Context.NativeCode = ErrorCode;
		Context.NativeMessage = ErrorMessage;
		return FOpenMobileAdsErrorMapper::FromNative(Context);
	}

	IOpenMobileAdsAdMobBackend* FindBackend()
	{
		TArray<IOpenMobileAdsAdMobBackend*> Backends =
			IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileAdsAdMobBackend>(
				IOpenMobileAdsAdMobBackend::GetModularFeatureName()
			);
		for (IOpenMobileAdsAdMobBackend* Backend : Backends)
		{
			if (Backend && Backend->IsAvailable())
			{
				return Backend;
			}
		}
		return nullptr;
	}

	bool BeginConsentFormOperation(
		const FOpenMobileAdsConsentRequest& Request,
		FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
		FOnOpenMobileAdMobConsentFailed&& OnFailed,
		bool bPrivacyOptions,
		FString& OutError
	)
	{
		IOpenMobileAdsAdMobBackend* Backend = FindBackend();
		if (!Backend)
		{
			OutError = TEXT("The AdMob provider has no native backend for UMP.");
			return false;
		}
		if (!Request.RequestId.IsValid())
		{
			OutError = TEXT("The Google UMP form request ID is invalid.");
			return false;
		}
		if (NativeConsentRequestIds.Contains(Request.RequestId))
		{
			OutError = TEXT("The Google UMP consent request is already active.");
			return false;
		}

		++NextRequestId;
		if (NextRequestId <= 0)
		{
			NextRequestId = 1;
		}
		const int64 NativeRequestId = NextRequestId;
		FConsentOperation Operation;
		Operation.RequestId = Request.RequestId;
		Operation.Completed = MoveTemp(OnCompleted);
		Operation.Failed = MoveTemp(OnFailed);
		ConsentOperations.Add(NativeRequestId, MoveTemp(Operation));
		NativeConsentRequestIds.Add(Request.RequestId, NativeRequestId);
		const bool bStarted = bPrivacyOptions
			? Backend->PresentPrivacyOptionsForm(NativeRequestId, OutError)
			: Backend->PresentRequiredConsentForm(NativeRequestId, OutError);
		if (!bStarted)
		{
			FConsentOperation Removed;
			RemoveConsentOperation(NativeRequestId, Removed);
		}
		return bStarted;
	}
}

bool FOpenMobileAdsAdMobPlatform::IsSupported()
{
	return OpenMobileAdsAdMobPlatformPrivate::FindBackend() != nullptr;
}

bool FOpenMobileAdsAdMobPlatform::Initialize(
	const FOpenMobileAdsInitializationRequest& Request,
	FOnOpenMobileAdMobInitializationStatus&& OnStatus,
	FOnOpenMobileAdMobInitialized&& OnCompleted,
	FString& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;

	IOpenMobileAdsAdMobBackend* Backend = FindBackend();
	if (!Backend)
	{
		OutError = TEXT("The AdMob provider has no native backend for this platform.");
		return false;
	}
	if (bInitialized)
	{
		for (const FOpenMobileAdsInitializationComponentStatus& Status : InitializationStatuses)
		{
			OnStatus.ExecuteIfBound(Status);
		}
		InitializationStatusDelegates.Add(MoveTemp(OnStatus));
		OnCompleted.ExecuteIfBound(FOpenMobileAdsError());
		return true;
	}

	InitializationStatusDelegates.Add(MoveTemp(OnStatus));
	InitializationDelegates.Add(MoveTemp(OnCompleted));
	if (bInitializationInProgress)
	{
		return true;
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	ActiveInitializationRequestId = NextRequestId;
	bInitializationInProgress = true;
	InitializationStatuses.Reset();
	if (!Backend->Initialize(Request, ActiveInitializationRequestId, OutError))
	{
		ResetInitialization();
		return false;
	}
	return true;
}

void FOpenMobileAdsAdMobPlatform::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	ResetRequest();
	ResetOperations();
	if (bInitialized || bInitializationInProgress)
	{
		if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
		{
			Backend->Shutdown();
		}
		bInitialized = false;
	}
	ResetInitialization();
}

bool FOpenMobileAdsAdMobPlatform::BeginConsentRefresh(
	const FOpenMobileAdsConsentRequest& Request,
	FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
	FOnOpenMobileAdMobConsentFailed&& OnFailed,
	FString& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	IOpenMobileAdsAdMobBackend* Backend = FindBackend();
	if (!Backend)
	{
		OutError = TEXT("The AdMob provider has no native backend for UMP.");
		return false;
	}
	if (!Request.RequestId.IsValid())
	{
		OutError = TEXT("The Google UMP refresh request ID is invalid.");
		return false;
	}
	if (NativeConsentRequestIds.Contains(Request.RequestId))
	{
		OutError = TEXT("The Google UMP consent request is already active.");
		return false;
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	const int64 NativeRequestId = NextRequestId;
	FConsentOperation Operation;
	Operation.RequestId = Request.RequestId;
	Operation.Completed = MoveTemp(OnCompleted);
	Operation.Failed = MoveTemp(OnFailed);
	ConsentOperations.Add(NativeRequestId, MoveTemp(Operation));
	NativeConsentRequestIds.Add(Request.RequestId, NativeRequestId);
	if (!Backend->RequestConsentInfo(Request, NativeRequestId, OutError))
	{
		FConsentOperation Removed;
		RemoveConsentOperation(NativeRequestId, Removed);
		return false;
	}
	return true;
}

bool FOpenMobileAdsAdMobPlatform::BeginRequiredConsentForm(
	const FOpenMobileAdsConsentRequest& Request,
	FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
	FOnOpenMobileAdMobConsentFailed&& OnFailed,
	FString& OutError
)
{
	check(IsInGameThread());
	return OpenMobileAdsAdMobPlatformPrivate::BeginConsentFormOperation(
		Request,
		MoveTemp(OnCompleted),
		MoveTemp(OnFailed),
		false,
		OutError
	);
}

bool FOpenMobileAdsAdMobPlatform::BeginPrivacyOptionsForm(
	const FOpenMobileAdsConsentRequest& Request,
	FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
	FOnOpenMobileAdMobConsentFailed&& OnFailed,
	FString& OutError
)
{
	check(IsInGameThread());
	return OpenMobileAdsAdMobPlatformPrivate::BeginConsentFormOperation(
		Request,
		MoveTemp(OnCompleted),
		MoveTemp(OnFailed),
		true,
		OutError
	);
}

void FOpenMobileAdsAdMobPlatform::CancelConsent(FGuid RequestId)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	int64 NativeRequestId = 0;
	if (NativeConsentRequestIds.RemoveAndCopyValue(RequestId, NativeRequestId))
	{
		ConsentOperations.Remove(NativeRequestId);
	}
}

bool FOpenMobileAdsAdMobPlatform::BeginLoad(
	const FOpenMobileAdsLoadRequest& Request,
	FOnOpenMobileAdMobRewardedCached&& OnLoaded,
	FOnOpenMobileAdMobRewardedFailed&& OnFailed,
	FString& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;

	IOpenMobileAdsAdMobBackend* Backend = FindBackend();
	if (!Backend)
	{
		OutError = TEXT("The AdMob provider has no native backend for this platform.");
		return false;
	}
	if (!bInitialized)
	{
		OutError = TEXT("The AdMob SDK has not finished initializing.");
		return false;
	}
	if (!Request.RequestId.IsValid())
	{
		OutError = TEXT("The AdMob rewarded load request ID is invalid.");
		return false;
	}
	if (Request.Placement.AdUnitId.IsEmpty())
	{
		OutError = TEXT("The AdMob rewarded-ad unit ID is empty.");
		return false;
	}
	if (NativeLoadRequestIds.Contains(Request.RequestId))
	{
		OutError = TEXT("The AdMob rewarded load request is already active.");
		return false;
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	const int64 NativeRequestId = NextRequestId;
	FRewardedLoadOperation Operation;
	Operation.RequestId = Request.RequestId;
	Operation.Loaded = MoveTemp(OnLoaded);
	Operation.Failed = MoveTemp(OnFailed);
	RewardedLoadOperations.Add(NativeRequestId, MoveTemp(Operation));
	NativeLoadRequestIds.Add(Request.RequestId, NativeRequestId);

	if (!Backend->LoadRewardedAd(
		Request.Placement.AdUnitId,
		NativeRequestId,
		Request.PrivacyContext.UsPrivacy.DataProcessingMode,
		OutError
	))
	{
		FRewardedLoadOperation Removed;
		RemoveLoadOperation(NativeRequestId, Removed);
		return false;
	}
	return true;
}

bool FOpenMobileAdsAdMobPlatform::BeginShow(
	const FOpenMobileAdsShowRequest& Request,
	TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
	FString& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;

	IOpenMobileAdsAdMobBackend* Backend = FindBackend();
	if (!Backend)
	{
		OutError = TEXT("The AdMob provider has no native backend for this platform.");
		return false;
	}
	if (!bInitialized)
	{
		OutError = TEXT("The AdMob SDK has not finished initializing.");
		return false;
	}
	if (!Request.RequestId.IsValid() || !Request.CachedAdId.IsValid())
	{
		OutError = TEXT("The AdMob rewarded show request or cache ID is invalid.");
		return false;
	}
	if (NativeShowRequestIds.Contains(Request.RequestId))
	{
		OutError = TEXT("The AdMob rewarded show request is already active.");
		return false;
	}

	int64 LoadedRequestId = 0;
	if (!LoadedRewardedAdRequestIds.RemoveAndCopyValue(
		Request.CachedAdId,
		LoadedRequestId
	))
	{
		OutError = TEXT("The AdMob rewarded cache is unavailable or already consumed.");
		return false;
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	const int64 NativeShowRequestId = NextRequestId;
	FRewardedShowOperation Operation;
	Operation.RequestId = Request.RequestId;
	Operation.LoadedRequestId = LoadedRequestId;
	Operation.EventSink = EventSink;
	RewardedShowOperations.Add(NativeShowRequestId, MoveTemp(Operation));
	NativeShowRequestIds.Add(Request.RequestId, NativeShowRequestId);

	if (!Backend->ShowRewardedAd(
		LoadedRequestId,
		NativeShowRequestId,
		Request.Options.ServerVerificationCustomData,
		OutError
	))
	{
		FRewardedShowOperation Removed;
		RemoveShowOperation(NativeShowRequestId, Removed);
		LoadedRewardedAdRequestIds.Add(Request.CachedAdId, LoadedRequestId);
		return false;
	}
	return true;
}

void FOpenMobileAdsAdMobPlatform::Cancel(FGuid RequestId)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	int64 NativeRequestId = 0;
	if (NativeLoadRequestIds.RemoveAndCopyValue(RequestId, NativeRequestId))
	{
		RewardedLoadOperations.Remove(NativeRequestId);
		if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
		{
			Backend->CancelRewardedAd(NativeRequestId);
		}
	}

	int64 NativeShowRequestId = 0;
	if (NativeShowRequestIds.RemoveAndCopyValue(RequestId, NativeShowRequestId))
	{
		FRewardedShowOperation Operation;
		if (RewardedShowOperations.RemoveAndCopyValue(
			NativeShowRequestId,
			Operation
		))
		{
			if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
			{
				Backend->CancelRewardedAd(Operation.LoadedRequestId);
			}
		}
	}
}

void FOpenMobileAdsAdMobPlatform::ReleaseCachedAd(FGuid CachedAdId)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	int64 NativeRequestId = 0;
	if (!LoadedRewardedAdRequestIds.RemoveAndCopyValue(CachedAdId, NativeRequestId))
	{
		return;
	}
	if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
	{
		Backend->CancelRewardedAd(NativeRequestId);
	}
}

bool FOpenMobileAdsAdMobPlatform::BeginRequest(
	const FString& AdUnitId,
	FOnOpenMobileAdMobRewardedLoaded&& OnLoaded,
	FOnOpenMobileAdMobRewardedShown&& OnShown,
	FOnOpenMobileAdMobRewardedEarned&& OnEarned,
	FOnOpenMobileAdMobRewardedClosed&& OnClosed,
	FOnOpenMobileAdMobRewardedFailed&& OnFailed,
	FString& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;

	IOpenMobileAdsAdMobBackend* Backend = FindBackend();
	if (!Backend)
	{
		OutError = TEXT("The AdMob provider has no native backend for this platform.");
		return false;
	}
	if (bRequestInProgress)
	{
		OutError = TEXT("An AdMob rewarded-ad request is already in progress.");
		return false;
	}
	if (AdUnitId.IsEmpty())
	{
		OutError = TEXT("The AdMob rewarded-ad unit ID is empty.");
		return false;
	}
	if (!bInitialized)
	{
		OutError = TEXT("The AdMob SDK has not finished initializing.");
		return false;
	}

	LoadedDelegate = MoveTemp(OnLoaded);
	ShownDelegate = MoveTemp(OnShown);
	EarnedDelegate = MoveTemp(OnEarned);
	ClosedDelegate = MoveTemp(OnClosed);
	FailedDelegate = MoveTemp(OnFailed);
	bRequestInProgress = true;
	bRewardDispatched = false;

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	ActiveRequestId = NextRequestId;

	if (!Backend->LaunchRewardedAd(AdUnitId, ActiveRequestId, OutError))
	{
		ResetRequest();
		return false;
	}

	return true;
}

void FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (!bInitializationInProgress || ActiveInitializationRequestId != RequestId)
		{
			return;
		}
		TArray<FOnOpenMobileAdMobInitialized> Completions = MoveTemp(InitializationDelegates);
		bInitializationInProgress = false;
		bInitialized = true;
		for (FOnOpenMobileAdMobInitialized& Completion : Completions)
		{
			Completion.ExecuteIfBound(FOpenMobileAdsError());
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeInitializationFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (!bInitializationInProgress || ActiveInitializationRequestId != RequestId)
		{
			return;
		}
		TArray<FOnOpenMobileAdMobInitialized> Completions = MoveTemp(InitializationDelegates);
		InitializationStatusDelegates.Reset();
		InitializationStatuses.Reset();
		ActiveInitializationRequestId = 0;
		bInitializationInProgress = false;
		bInitialized = false;
		for (FOnOpenMobileAdMobInitialized& Completion : Completions)
		{
			Completion.ExecuteIfBound(FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Initialization,
				NAME_None,
				ErrorMessage.IsEmpty()
					? TEXT("The AdMob SDK failed to initialize.")
					: ErrorMessage,
				TEXT("AdMob")
			));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeConsentInfoUpdated(
	int64 RequestId,
	int32 ConsentStatus,
	bool bCanRequestAds,
	int32 PrivacyOptionsRequirement
)
{
	OpenMobile::DispatchToGameThread([
		RequestId,
		ConsentStatus,
		bCanRequestAds,
		PrivacyOptionsRequirement
	]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FConsentOperation Operation;
		if (RemoveConsentOperation(RequestId, Operation))
		{
			Operation.Completed.ExecuteIfBound(MakeConsentUpdate(
				ConsentStatus,
				bCanRequestAds,
				PrivacyOptionsRequirement
			));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeConsentFormDismissed(
	int64 RequestId,
	int32 ConsentStatus,
	bool bCanRequestAds,
	int32 PrivacyOptionsRequirement
)
{
	OpenMobile::DispatchToGameThread([
		RequestId,
		ConsentStatus,
		bCanRequestAds,
		PrivacyOptionsRequirement
	]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FConsentOperation Operation;
		if (RemoveConsentOperation(RequestId, Operation))
		{
			Operation.Completed.ExecuteIfBound(MakeConsentUpdate(
				ConsentStatus,
				bCanRequestAds,
				PrivacyOptionsRequirement
			));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeConsentFailed(
	int64 RequestId,
	FString ErrorCode,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread([
		RequestId,
		ErrorCode = MoveTemp(ErrorCode),
		ErrorMessage = MoveTemp(ErrorMessage)
	]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FConsentOperation Operation;
		if (RemoveConsentOperation(RequestId, Operation))
		{
			Operation.Failed.ExecuteIfBound(MakeConsentError(
				ErrorCode,
				ErrorMessage
			));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeAdapterInitializationStatus(
	int64 RequestId,
	FString AdapterName,
	bool bReady,
	double LatencyMilliseconds,
	FString Description
)
{
	OpenMobile::DispatchToGameThread(
		[
			RequestId,
			AdapterName = MoveTemp(AdapterName),
			bReady,
			LatencyMilliseconds,
			Description = MoveTemp(Description)
		]() mutable
		{
			using namespace OpenMobileAdsAdMobPlatformPrivate;
			if (
				ActiveInitializationRequestId != RequestId
				|| (!bInitializationInProgress && !bInitialized)
				|| AdapterName.IsEmpty()
			)
			{
				return;
			}

			FOpenMobileAdsInitializationComponentStatus Status;
			Status.Type = EOpenMobileAdsInitializationComponentType::Adapter;
			Status.Name = FName(*AdapterName);
			Status.Parent = TEXT("AdMob");
			Status.State = bReady
				? EOpenMobileAdsInitializationState::Ready
				: EOpenMobileAdsInitializationState::Failed;
			Status.LatencyMilliseconds = FMath::Max(0.0, LatencyMilliseconds);
			if (!bReady)
			{
				Status.Error = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::ProviderFailure,
					EOpenMobileAdsFailureStage::Initialization,
					NAME_None,
					Description.IsEmpty()
						? TEXT("The AdMob adapter did not initialize.")
						: MoveTemp(Description),
					TEXT("AdMob")
				);
			}
			BroadcastInitializationStatus(MoveTemp(Status));
		}
	);
}

void FOpenMobileAdsAdMobPlatform::NativeRewardedLoadCompleted(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FRewardedLoadOperation Operation;
		if (RemoveLoadOperation(RequestId, Operation))
		{
			const FGuid CachedAdId = FGuid::NewGuid();
			LoadedRewardedAdRequestIds.Add(CachedAdId, RequestId);
			Operation.Loaded.ExecuteIfBound(CachedAdId);
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeRewardedLoadFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FRewardedLoadOperation Operation;
		if (RemoveLoadOperation(RequestId, Operation))
		{
			Operation.Failed.ExecuteIfBound(MoveTemp(ErrorMessage));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeLoaded(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (bRequestInProgress && ActiveRequestId == RequestId)
		{
			LoadedDelegate.ExecuteIfBound();
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeShown(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (FRewardedShowOperation* Operation = RewardedShowOperations.Find(RequestId))
		{
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::Shown;
			Operation->EventSink->Submit(MoveTemp(Event));
			return;
		}
		if (bRequestInProgress && ActiveRequestId == RequestId)
		{
			ShownDelegate.ExecuteIfBound();
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeImpression(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (FRewardedShowOperation* Operation = RewardedShowOperations.Find(RequestId))
		{
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::Impression;
			Operation->EventSink->Submit(MoveTemp(Event));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeClicked(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (FRewardedShowOperation* Operation = RewardedShowOperations.Find(RequestId))
		{
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::Clicked;
			Operation->EventSink->Submit(MoveTemp(Event));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
	int64 RequestId,
	int64 ValueMicros,
	FString CurrencyCode,
	int32 Precision
)
{
	OpenMobile::DispatchToGameThread(
		[RequestId, ValueMicros, CurrencyCode = MoveTemp(CurrencyCode), Precision]() mutable
		{
			using namespace OpenMobileAdsAdMobPlatformPrivate;
			FRewardedShowOperation* Operation = RewardedShowOperations.Find(RequestId);
			if (!Operation)
			{
				return;
			}
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::RevenuePaid;
			Event.bHasRevenue = true;
			Event.Revenue.ValueMicros = ValueMicros;
			Event.Revenue.CurrencyCode = MoveTemp(CurrencyCode);
			Event.Revenue.Precision = Precision >= 0
				&& Precision <= static_cast<int32>(EOpenMobileAdsRevenuePrecision::Precise)
				? static_cast<EOpenMobileAdsRevenuePrecision>(Precision)
				: EOpenMobileAdsRevenuePrecision::Unknown;
			Operation->EventSink->Submit(MoveTemp(Event));
		}
	);
}

void FOpenMobileAdsAdMobPlatform::NativeEarned(
	int64 RequestId,
	int32 NetworkAmount,
	FString NetworkRewardType
)
{
	OpenMobile::DispatchToGameThread(
		[RequestId, NetworkAmount, NetworkRewardType = MoveTemp(NetworkRewardType)]() mutable
		{
			using namespace OpenMobileAdsAdMobPlatformPrivate;
			if (FRewardedShowOperation* Operation = RewardedShowOperations.Find(RequestId))
			{
				if (Operation->bRewardDispatched)
				{
					return;
				}
				Operation->bRewardDispatched = true;
				FOpenMobileAdsEvent Event;
				Event.Type = EOpenMobileAdsEventType::RewardEarned;
				Event.bHasReward = NetworkAmount > 0;
				Event.Reward.Amount = static_cast<int64>(NetworkAmount);
				Event.Reward.Type = MoveTemp(NetworkRewardType);
				Operation->EventSink->Submit(MoveTemp(Event));
				return;
			}
			if (!bRequestInProgress || ActiveRequestId != RequestId || bRewardDispatched)
			{
				return;
			}
			bRewardDispatched = true;
			EarnedDelegate.ExecuteIfBound(NetworkAmount, MoveTemp(NetworkRewardType));
		}
	);
}

void FOpenMobileAdsAdMobPlatform::NativeClosed(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FRewardedShowOperation ShowOperation;
		if (RemoveShowOperation(RequestId, ShowOperation))
		{
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::Dismissed;
			ShowOperation.EventSink->Submit(MoveTemp(Event));
			return;
		}
		if (!bRequestInProgress || ActiveRequestId != RequestId)
		{
			return;
		}
		FOnOpenMobileAdMobRewardedClosed Completion = MoveTemp(ClosedDelegate);
		ResetRequest();
		Completion.ExecuteIfBound();
	});
}

void FOpenMobileAdsAdMobPlatform::NativeFailed(int64 RequestId, FString ErrorMessage)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FRewardedShowOperation ShowOperation;
		if (RemoveShowOperation(RequestId, ShowOperation))
		{
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::Failed;
			Event.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Show,
				NAME_None,
				ErrorMessage.IsEmpty()
					? TEXT("AdMob failed to present the rewarded ad.")
					: MoveTemp(ErrorMessage),
				TEXT("AdMob")
			);
			ShowOperation.EventSink->Submit(MoveTemp(Event));
			return;
		}
		if (!bRequestInProgress || ActiveRequestId != RequestId)
		{
			return;
		}
		FOnOpenMobileAdMobRewardedFailed Completion = MoveTemp(FailedDelegate);
		ResetRequest();
		Completion.ExecuteIfBound(MoveTemp(ErrorMessage));
	});
}

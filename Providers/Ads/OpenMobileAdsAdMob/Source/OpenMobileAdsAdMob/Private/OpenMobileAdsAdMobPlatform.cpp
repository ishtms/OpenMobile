#include "OpenMobileAdsAdMobPlatform.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsAdMobBackend.h"
#include "IOpenMobileAdsProvider.h"
#include "OpenMobileAsync.h"
#include "OpenMobileAdsAdMobConsentMapper.h"

namespace OpenMobileAdsAdMobPlatformPrivate
{
	struct FAdLoadOperation
	{
		FGuid RequestId;
		EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;
		FOnOpenMobileAdMobAdCached Loaded;
		FOnOpenMobileAdMobAdLoadFailed Failed;
	};

	struct FShowOperation
	{
		FGuid RequestId;
		FGuid CachedAdId;
		EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;
		int64 LoadedRequestId = 0;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink;
		bool bRewardDispatched = false;
	};

	struct FBannerHideOperation
	{
		FGuid RequestId;
		FGuid CachedAdId;
		int64 LoadedRequestId = 0;
		int64 ShowRequestId = 0;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink;
	};

	struct FCachedAdReference
	{
		EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;
		int64 LoadedRequestId = 0;
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
	TMap<int64, FAdLoadOperation> LoadOperations;
	TMap<int64, FShowOperation> ShowOperations;
	TMap<int64, FBannerHideOperation> BannerHideOperations;
	TMap<FGuid, int64> NativeLoadRequestIds;
	TMap<FGuid, int64> NativeShowRequestIds;
	TMap<FGuid, int64> NativeBannerHideRequestIds;
	TMap<FGuid, int64> ActiveBannerShowRequestIds;
	TMap<FGuid, int64> ActiveBannerHideRequestIds;
	TMap<FGuid, FCachedAdReference> LoadedAds;
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
		LoadOperations.Reset();
		ShowOperations.Reset();
		BannerHideOperations.Reset();
		NativeLoadRequestIds.Reset();
		NativeShowRequestIds.Reset();
		NativeBannerHideRequestIds.Reset();
		ActiveBannerShowRequestIds.Reset();
		ActiveBannerHideRequestIds.Reset();
		LoadedAds.Reset();
		ConsentOperations.Reset();
		NativeConsentRequestIds.Reset();
	}

	bool RemoveLoadOperation(
		int64 NativeRequestId,
		FAdLoadOperation& OutOperation
	)
	{
		if (!LoadOperations.RemoveAndCopyValue(NativeRequestId, OutOperation))
		{
			return false;
		}
		NativeLoadRequestIds.Remove(OutOperation.RequestId);
		return true;
	}

	bool RemoveLoadOperationForFormat(
		int64 NativeRequestId,
		EOpenMobileAdFormat Format,
		FAdLoadOperation& OutOperation
	)
	{
		const FAdLoadOperation* Operation = LoadOperations.Find(NativeRequestId);
		return Operation
			&& Operation->Format == Format
			&& RemoveLoadOperation(NativeRequestId, OutOperation);
	}

	bool RemoveBannerLoadOperation(
		int64 NativeRequestId,
		FAdLoadOperation& OutOperation
	)
	{
		const FAdLoadOperation* Operation = LoadOperations.Find(NativeRequestId);
		return Operation
			&& (
				Operation->Format == EOpenMobileAdFormat::Banner
				|| Operation->Format
					== EOpenMobileAdFormat::AnchoredAdaptiveBanner
				|| Operation->Format == EOpenMobileAdFormat::MediumRectangle
			)
			&& RemoveLoadOperation(NativeRequestId, OutOperation);
	}

	bool RemoveShowOperation(
		int64 NativeRequestId,
		FShowOperation& OutOperation
	)
	{
		if (!ShowOperations.RemoveAndCopyValue(NativeRequestId, OutOperation))
		{
			return false;
		}
		NativeShowRequestIds.Remove(OutOperation.RequestId);
		ActiveBannerShowRequestIds.Remove(OutOperation.CachedAdId);
		return true;
	}

	bool RemoveBannerHideOperation(
		int64 NativeRequestId,
		FBannerHideOperation& OutOperation
	)
	{
		if (!BannerHideOperations.RemoveAndCopyValue(
			NativeRequestId,
			OutOperation
		))
		{
			return false;
		}
		NativeBannerHideRequestIds.Remove(OutOperation.RequestId);
		ActiveBannerHideRequestIds.Remove(OutOperation.CachedAdId);
		return true;
	}

	bool LoadNativeAd(
		IOpenMobileAdsAdMobBackend& Backend,
		const FOpenMobileAdsLoadRequest& Request,
		int64 NativeRequestId,
		FString& OutError
	)
	{
		switch (Request.Placement.Format)
		{
		case EOpenMobileAdFormat::Banner:
		case EOpenMobileAdFormat::AnchoredAdaptiveBanner:
		case EOpenMobileAdFormat::MediumRectangle:
			return Backend.LoadBannerAd(
				Request.Placement.AdUnitId,
				NativeRequestId,
				Request.PrivacyContext.UsPrivacy.DataProcessingMode,
				Request.Placement.Format,
				Request.Placement.BannerLayout,
				OutError
			);
		case EOpenMobileAdFormat::Interstitial:
			return Backend.LoadInterstitialAd(
				Request.Placement.AdUnitId,
				NativeRequestId,
				Request.PrivacyContext.UsPrivacy.DataProcessingMode,
				OutError
			);
		case EOpenMobileAdFormat::Rewarded:
			return Backend.LoadRewardedAd(
				Request.Placement.AdUnitId,
				NativeRequestId,
				Request.PrivacyContext.UsPrivacy.DataProcessingMode,
				OutError
			);
		case EOpenMobileAdFormat::RewardedInterstitial:
			return Backend.LoadRewardedInterstitialAd(
				Request.Placement.AdUnitId,
				NativeRequestId,
				Request.PrivacyContext.UsPrivacy.DataProcessingMode,
				OutError
			);
		case EOpenMobileAdFormat::AppOpen:
			return Backend.LoadAppOpenAd(
				Request.Placement.AdUnitId,
				NativeRequestId,
				Request.PrivacyContext.UsPrivacy.DataProcessingMode,
				OutError
			);
		default:
			OutError = TEXT("The AdMob native backend does not support this load format.");
			return false;
		}
	}

	void CancelNativeAd(
		IOpenMobileAdsAdMobBackend& Backend,
		EOpenMobileAdFormat Format,
		int64 NativeRequestId
	)
	{
		if (
			Format == EOpenMobileAdFormat::Banner
			|| Format == EOpenMobileAdFormat::AnchoredAdaptiveBanner
			|| Format == EOpenMobileAdFormat::MediumRectangle
		)
		{
			Backend.CancelBannerAd(NativeRequestId);
		}
		else if (Format == EOpenMobileAdFormat::Interstitial)
		{
			Backend.CancelInterstitialAd(NativeRequestId);
		}
		else if (Format == EOpenMobileAdFormat::Rewarded)
		{
			Backend.CancelRewardedAd(NativeRequestId);
		}
		else if (Format == EOpenMobileAdFormat::RewardedInterstitial)
		{
			Backend.CancelRewardedInterstitialAd(NativeRequestId);
		}
		else if (Format == EOpenMobileAdFormat::AppOpen)
		{
			Backend.CancelAppOpenAd(NativeRequestId);
		}
	}

	bool ShowNativeAd(
		IOpenMobileAdsAdMobBackend& Backend,
		const FOpenMobileAdsShowRequest& Request,
		int64 LoadedRequestId,
		int64 NativeShowRequestId,
		FString& OutError
	)
	{
		if (
			Request.Format == EOpenMobileAdFormat::Banner
			|| Request.Format == EOpenMobileAdFormat::AnchoredAdaptiveBanner
			|| Request.Format == EOpenMobileAdFormat::MediumRectangle
		)
		{
			return Backend.ShowBannerAd(
				LoadedRequestId,
				NativeShowRequestId,
				Request.BannerLayout,
				OutError
			);
		}
		if (Request.Format == EOpenMobileAdFormat::Interstitial)
		{
			return Backend.ShowInterstitialAd(
				LoadedRequestId,
				NativeShowRequestId,
				OutError
			);
		}
		if (Request.Format == EOpenMobileAdFormat::Rewarded)
		{
			return Backend.ShowRewardedAd(
				LoadedRequestId,
				NativeShowRequestId,
				Request.Options.ServerVerificationCustomData,
				OutError
			);
		}
		if (Request.Format == EOpenMobileAdFormat::RewardedInterstitial)
		{
			return Backend.ShowRewardedInterstitialAd(
				LoadedRequestId,
				NativeShowRequestId,
				Request.Options.ServerVerificationCustomData,
				OutError
			);
		}
		if (Request.Format == EOpenMobileAdFormat::AppOpen)
		{
			return Backend.ShowAppOpenAd(
				LoadedRequestId,
				NativeShowRequestId,
				OutError
			);
		}
		OutError = TEXT("The AdMob native backend does not support this show format.");
		return false;
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

bool FOpenMobileAdsAdMobPlatform::ResetConsentForTesting(FString& OutError)
{
	check(IsInGameThread());
	IOpenMobileAdsAdMobBackend* Backend =
		OpenMobileAdsAdMobPlatformPrivate::FindBackend();
	if (!Backend)
	{
		OutError = TEXT("The AdMob provider has no native backend for consent reset.");
		return false;
	}
	return Backend->ResetConsentForTesting(OutError);
}

bool FOpenMobileAdsAdMobPlatform::ApplyConsentSignals(
	const FOpenMobileAdsConsentSignals& Signals,
	int32 SignalMask,
	FString& OutError
)
{
	check(IsInGameThread());
	if (SignalMask == 0)
	{
		return true;
	}
	IOpenMobileAdsAdMobBackend* Backend =
		OpenMobileAdsAdMobPlatformPrivate::FindBackend();
	if (!Backend)
	{
		OutError = TEXT("The AdMob provider has no native backend for consent signals.");
		return false;
	}
	return Backend->ApplyConsentSignals(Signals, SignalMask, OutError);
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
	FOnOpenMobileAdMobAdCached&& OnLoaded,
	FOnOpenMobileAdMobAdLoadFailed&& OnFailed,
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
		OutError = TEXT("The AdMob load request ID is invalid.");
		return false;
	}
	if (Request.Placement.AdUnitId.IsEmpty())
	{
		OutError = TEXT("The AdMob ad-unit ID is empty.");
		return false;
	}
	if (NativeLoadRequestIds.Contains(Request.RequestId))
	{
		OutError = TEXT("The AdMob load request is already active.");
		return false;
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	const int64 NativeRequestId = NextRequestId;
	FAdLoadOperation Operation;
	Operation.RequestId = Request.RequestId;
	Operation.Format = Request.Placement.Format;
	Operation.Loaded = MoveTemp(OnLoaded);
	Operation.Failed = MoveTemp(OnFailed);
	LoadOperations.Add(NativeRequestId, MoveTemp(Operation));
	NativeLoadRequestIds.Add(Request.RequestId, NativeRequestId);

	if (!LoadNativeAd(*Backend, Request, NativeRequestId, OutError))
	{
		FAdLoadOperation Removed;
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
		OutError = TEXT("The AdMob show request or cache ID is invalid.");
		return false;
	}
	if (NativeShowRequestIds.Contains(Request.RequestId))
	{
		OutError = TEXT("The AdMob show request is already active.");
		return false;
	}

	const FCachedAdReference* CachedReference = LoadedAds.Find(Request.CachedAdId);
	if (!CachedReference || CachedReference->Format != Request.Format)
	{
		OutError = TEXT("The AdMob cache is unavailable, mismatched, or already consumed.");
		return false;
	}
	const FCachedAdReference Reference = *CachedReference;
	const bool bPersistentBanner = Request.Format == EOpenMobileAdFormat::Banner
		|| Request.Format == EOpenMobileAdFormat::AnchoredAdaptiveBanner
		|| Request.Format == EOpenMobileAdFormat::MediumRectangle;
	if (
		bPersistentBanner
		&& (
			ActiveBannerShowRequestIds.Contains(Request.CachedAdId)
			|| ActiveBannerHideRequestIds.Contains(Request.CachedAdId)
		)
	)
	{
		OutError = TEXT("The AdMob banner cache is already showing or hiding.");
		return false;
	}
	if (!bPersistentBanner)
	{
		LoadedAds.Remove(Request.CachedAdId);
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	const int64 NativeShowRequestId = NextRequestId;
	FShowOperation Operation;
	Operation.RequestId = Request.RequestId;
	Operation.CachedAdId = Request.CachedAdId;
	Operation.Format = Request.Format;
	Operation.LoadedRequestId = Reference.LoadedRequestId;
	Operation.EventSink = EventSink;
	ShowOperations.Add(NativeShowRequestId, MoveTemp(Operation));
	NativeShowRequestIds.Add(Request.RequestId, NativeShowRequestId);
	if (bPersistentBanner)
	{
		ActiveBannerShowRequestIds.Add(Request.CachedAdId, NativeShowRequestId);
	}

	if (!ShowNativeAd(
		*Backend,
		Request,
		Reference.LoadedRequestId,
		NativeShowRequestId,
		OutError
	))
	{
		FShowOperation Removed;
		RemoveShowOperation(NativeShowRequestId, Removed);
		if (!bPersistentBanner)
		{
			LoadedAds.Add(Request.CachedAdId, Reference);
		}
		return false;
	}
	return true;
}

bool FOpenMobileAdsAdMobPlatform::BeginHide(
	const FOpenMobileAdsHideRequest& Request,
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
		OutError = TEXT("The AdMob hide request or cache ID is invalid.");
		return false;
	}
	if (
		NativeBannerHideRequestIds.Contains(Request.RequestId)
		|| ActiveBannerHideRequestIds.Contains(Request.CachedAdId)
	)
	{
		OutError = TEXT("The AdMob banner hide request is already active.");
		return false;
	}

	const FCachedAdReference* CachedReference = LoadedAds.Find(Request.CachedAdId);
	const int64* NativeShowRequestId = ActiveBannerShowRequestIds.Find(
		Request.CachedAdId
	);
	if (
		!CachedReference
		|| (
			CachedReference->Format != EOpenMobileAdFormat::Banner
			&& CachedReference->Format
				!= EOpenMobileAdFormat::AnchoredAdaptiveBanner
			&& CachedReference->Format != EOpenMobileAdFormat::MediumRectangle
		)
		|| !NativeShowRequestId
		|| !ShowOperations.Contains(*NativeShowRequestId)
	)
	{
		OutError = TEXT("The AdMob banner cache is unavailable or not visible.");
		return false;
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	const int64 NativeHideRequestId = NextRequestId;
	FBannerHideOperation Operation;
	Operation.RequestId = Request.RequestId;
	Operation.CachedAdId = Request.CachedAdId;
	Operation.LoadedRequestId = CachedReference->LoadedRequestId;
	Operation.ShowRequestId = *NativeShowRequestId;
	Operation.EventSink = EventSink;
	BannerHideOperations.Add(NativeHideRequestId, MoveTemp(Operation));
	NativeBannerHideRequestIds.Add(Request.RequestId, NativeHideRequestId);
	ActiveBannerHideRequestIds.Add(Request.CachedAdId, NativeHideRequestId);

	if (!Backend->HideBannerAd(
		CachedReference->LoadedRequestId,
		NativeHideRequestId,
		OutError
	))
	{
		FBannerHideOperation Removed;
		RemoveBannerHideOperation(NativeHideRequestId, Removed);
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
		FAdLoadOperation Operation;
		if (LoadOperations.RemoveAndCopyValue(NativeRequestId, Operation))
		{
			if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
			{
				CancelNativeAd(*Backend, Operation.Format, NativeRequestId);
			}
		}
	}

	int64 NativeShowRequestId = 0;
	if (NativeShowRequestIds.RemoveAndCopyValue(RequestId, NativeShowRequestId))
	{
		FShowOperation Operation;
		if (RemoveShowOperation(NativeShowRequestId, Operation))
		{
			if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
			{
				CancelNativeAd(
					*Backend,
					Operation.Format,
					Operation.LoadedRequestId
				);
			}
		}
	}

	int64 NativeHideRequestId = 0;
	if (NativeBannerHideRequestIds.RemoveAndCopyValue(
		RequestId,
		NativeHideRequestId
	))
	{
		FBannerHideOperation Operation;
		if (RemoveBannerHideOperation(NativeHideRequestId, Operation))
		{
			FShowOperation ShowOperation;
			RemoveShowOperation(Operation.ShowRequestId, ShowOperation);
			if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
			{
				Backend->CancelBannerAd(Operation.LoadedRequestId);
			}
		}
	}
}

void FOpenMobileAdsAdMobPlatform::ReleaseCachedAd(FGuid CachedAdId)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	FCachedAdReference Reference;
	if (!LoadedAds.RemoveAndCopyValue(CachedAdId, Reference))
	{
		return;
	}
	int64 NativeHideRequestId = 0;
	if (ActiveBannerHideRequestIds.RemoveAndCopyValue(
		CachedAdId,
		NativeHideRequestId
	))
	{
		FBannerHideOperation HideOperation;
		RemoveBannerHideOperation(NativeHideRequestId, HideOperation);
	}
	int64 NativeShowRequestId = 0;
	if (ActiveBannerShowRequestIds.RemoveAndCopyValue(
		CachedAdId,
		NativeShowRequestId
	))
	{
		FShowOperation ShowOperation;
		RemoveShowOperation(NativeShowRequestId, ShowOperation);
	}
	if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
	{
		CancelNativeAd(*Backend, Reference.Format, Reference.LoadedRequestId);
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
		FAdLoadOperation Operation;
		if (RemoveLoadOperationForFormat(
			RequestId,
			EOpenMobileAdFormat::Rewarded,
			Operation
		))
		{
			const FGuid CachedAdId = FGuid::NewGuid();
			FCachedAdReference Reference;
			Reference.Format = Operation.Format;
			Reference.LoadedRequestId = RequestId;
			LoadedAds.Add(CachedAdId, Reference);
			Operation.Loaded.ExecuteIfBound(CachedAdId, 0, FString());
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
		FAdLoadOperation Operation;
		if (RemoveLoadOperationForFormat(
			RequestId,
			EOpenMobileAdFormat::Rewarded,
			Operation
		))
		{
			Operation.Failed.ExecuteIfBound(MoveTemp(ErrorMessage));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadCompleted(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FAdLoadOperation Operation;
		if (RemoveLoadOperationForFormat(
			RequestId,
			EOpenMobileAdFormat::Interstitial,
			Operation
		))
		{
			const FGuid CachedAdId = FGuid::NewGuid();
			FCachedAdReference Reference;
			Reference.Format = Operation.Format;
			Reference.LoadedRequestId = RequestId;
			LoadedAds.Add(CachedAdId, Reference);
			Operation.Loaded.ExecuteIfBound(CachedAdId, 0, FString());
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadCompleted(
	int64 RequestId,
	int64 RewardAmount,
	FString RewardType
)
{
	OpenMobile::DispatchToGameThread(
		[RequestId, RewardAmount, RewardType = MoveTemp(RewardType)]() mutable
		{
			using namespace OpenMobileAdsAdMobPlatformPrivate;
			FAdLoadOperation Operation;
			if (RemoveLoadOperationForFormat(
				RequestId,
				EOpenMobileAdFormat::RewardedInterstitial,
				Operation
			))
			{
				const FGuid CachedAdId = FGuid::NewGuid();
				FCachedAdReference Reference;
				Reference.Format = Operation.Format;
				Reference.LoadedRequestId = RequestId;
				LoadedAds.Add(CachedAdId, Reference);
				Operation.Loaded.ExecuteIfBound(
					CachedAdId,
					RewardAmount,
					MoveTemp(RewardType)
				);
			}
		}
	);
}

void FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread(
		[RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
		{
			using namespace OpenMobileAdsAdMobPlatformPrivate;
			FAdLoadOperation Operation;
			if (RemoveLoadOperationForFormat(
				RequestId,
				EOpenMobileAdFormat::RewardedInterstitial,
				Operation
			))
			{
				Operation.Failed.ExecuteIfBound(MoveTemp(ErrorMessage));
			}
		}
	);
}

void FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FAdLoadOperation Operation;
		if (RemoveLoadOperationForFormat(
			RequestId,
			EOpenMobileAdFormat::Interstitial,
			Operation
		))
		{
			Operation.Failed.ExecuteIfBound(MoveTemp(ErrorMessage));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadCompleted(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FAdLoadOperation Operation;
		if (RemoveLoadOperationForFormat(
			RequestId,
			EOpenMobileAdFormat::AppOpen,
			Operation
		))
		{
			const FGuid CachedAdId = FGuid::NewGuid();
			FCachedAdReference Reference;
			Reference.Format = Operation.Format;
			Reference.LoadedRequestId = RequestId;
			LoadedAds.Add(CachedAdId, Reference);
			Operation.Loaded.ExecuteIfBound(CachedAdId, 0, FString());
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread(
		[RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
		{
			using namespace OpenMobileAdsAdMobPlatformPrivate;
			FAdLoadOperation Operation;
			if (RemoveLoadOperationForFormat(
				RequestId,
				EOpenMobileAdFormat::AppOpen,
				Operation
			))
			{
				Operation.Failed.ExecuteIfBound(MoveTemp(ErrorMessage));
			}
		}
	);
}

void FOpenMobileAdsAdMobPlatform::NativeBannerLoadCompleted(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FAdLoadOperation Operation;
		if (RemoveBannerLoadOperation(RequestId, Operation))
		{
			const FGuid CachedAdId = FGuid::NewGuid();
			FCachedAdReference Reference;
			Reference.Format = Operation.Format;
			Reference.LoadedRequestId = RequestId;
			LoadedAds.Add(CachedAdId, Reference);
			Operation.Loaded.ExecuteIfBound(CachedAdId, 0, FString());
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeBannerLoadFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FAdLoadOperation Operation;
		if (RemoveBannerLoadOperation(RequestId, Operation))
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
		if (FShowOperation* Operation = ShowOperations.Find(RequestId))
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

void FOpenMobileAdsAdMobPlatform::NativeBannerShown(int64 RequestId)
{
	NativeShown(RequestId);
}

void FOpenMobileAdsAdMobPlatform::NativeBannerHidden(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FBannerHideOperation HideOperation;
		if (!RemoveBannerHideOperation(RequestId, HideOperation))
		{
			return;
		}
		FShowOperation ShowOperation;
		RemoveShowOperation(HideOperation.ShowRequestId, ShowOperation);
		FOpenMobileAdsEvent Event;
		Event.Type = EOpenMobileAdsEventType::Hidden;
		HideOperation.EventSink->Submit(MoveTemp(Event));
	});
}

void FOpenMobileAdsAdMobPlatform::NativeBannerOperationFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FBannerHideOperation HideOperation;
		if (RemoveBannerHideOperation(RequestId, HideOperation))
		{
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::Failed;
			Event.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Hide,
				NAME_None,
				ErrorMessage.IsEmpty()
					? TEXT("AdMob failed to hide the banner ad.")
					: MoveTemp(ErrorMessage),
				TEXT("AdMob")
			);
			HideOperation.EventSink->Submit(MoveTemp(Event));
			return;
		}

		FShowOperation ShowOperation;
		if (RemoveShowOperation(RequestId, ShowOperation))
		{
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::Failed;
			Event.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Show,
				NAME_None,
				ErrorMessage.IsEmpty()
					? TEXT("AdMob failed to show the banner ad.")
					: MoveTemp(ErrorMessage),
				TEXT("AdMob")
			);
			ShowOperation.EventSink->Submit(MoveTemp(Event));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeImpression(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (FShowOperation* Operation = ShowOperations.Find(RequestId))
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
		if (FShowOperation* Operation = ShowOperations.Find(RequestId))
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
			FShowOperation* Operation = ShowOperations.Find(RequestId);
			int64 NormalizedValueMicros = 0;
			if (
				!Operation
				|| !FOpenMobileAdsRevenue::TryScaleToMicros(
					ValueMicros,
					1,
					NormalizedValueMicros
				)
			)
			{
				return;
			}
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::RevenuePaid;
			Event.bHasRevenue = true;
			Event.Revenue.ValueMicros = NormalizedValueMicros;
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
			if (FShowOperation* Operation = ShowOperations.Find(RequestId))
			{
				if (
					(
						Operation->Format != EOpenMobileAdFormat::Rewarded
						&& Operation->Format
							!= EOpenMobileAdFormat::RewardedInterstitial
					)
					|| Operation->bRewardDispatched
				)
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
		FShowOperation ShowOperation;
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
		FShowOperation ShowOperation;
		if (RemoveShowOperation(RequestId, ShowOperation))
		{
			FOpenMobileAdsEvent Event;
			Event.Type = EOpenMobileAdsEventType::Failed;
			Event.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Show,
				NAME_None,
				ErrorMessage.IsEmpty()
					? TEXT("AdMob failed to present the full-screen ad.")
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

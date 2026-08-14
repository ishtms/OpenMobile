#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileAdsOperations.h"

/** Keeps the AdMob provider independent from Android and iOS SDK headers. */
class IOpenMobileAdsAdMobBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileAdsAdMobBackend() = default;

	/** Uses one modular feature key so only the active platform backend is selected. */
	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Ads.AdMob.Backend"));
		return FeatureName;
	}

	/** Returns the stable platform name used for deterministic backend selection. */
	virtual FName GetBackendName() const = 0;
	/** Reports runtime SDK availability without initializing Google Mobile Ads. */
	virtual bool IsAvailable() const = 0;
	/** Starts native SDK setup with a numeric identity that platform callbacks can return unchanged. */
	virtual bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		int64 RequestId,
		FString& OutError
	) = 0;
	/** Disconnects native callbacks and releases every platform-side cached ad. */
	virtual void Shutdown() = 0;
	/** Refreshes UMP consent information without presenting a form by itself. */
	virtual bool RequestConsentInfo(
		const FOpenMobileAdsConsentRequest& Request,
		int64 RequestId,
		FString& OutError
	) = 0;
	/** Presents the required UMP form after a successful information refresh. */
	virtual bool PresentRequiredConsentForm(
		int64 RequestId,
		FString& OutError
	) = 0;
	/** Presents Google's user-invoked privacy options form when UMP reports it available. */
	virtual bool PresentPrivacyOptionsForm(
		int64 RequestId,
		FString& OutError
	) = 0;
	/** Clears UMP test state only through the provider's development reset path. */
	virtual bool ResetConsentForTesting(FString& OutError) = 0;
	/** Applies normalized privacy bits before SDK initialization or through allowed runtime updates. */
	virtual bool ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask,
		FString& OutError
	) = 0;
	/** Loads one rewarded ad with the request's resolved data-processing mode. */
	virtual bool LoadRewardedAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) = 0;
	/** Loads one interstitial without sharing native ownership with rewarded objects. */
	virtual bool LoadInterstitialAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) = 0;
	/** Loads one rewarded interstitial and keeps its reward metadata tied to the load identity. */
	virtual bool LoadRewardedInterstitialAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) = 0;
	/** Loads one App Open ad without presenting it from a lifecycle callback. */
	virtual bool LoadAppOpenAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) = 0;
	/** Loads one persistent display format using the resolved size and safe-area layout. */
	virtual bool LoadBannerAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		EOpenMobileAdFormat Format,
		const FOpenMobileAdsBannerLayout& Layout,
		FString& OutError
	) = 0;
	/** Cancels only the matching rewarded load while preserving a completed cache entry. */
	virtual void CancelRewardedAd(int64 RequestId) = 0;
	/** Cancels only the matching interstitial load identity. */
	virtual void CancelInterstitialAd(int64 RequestId) = 0;
	/** Cancels only the matching rewarded-interstitial load identity. */
	virtual void CancelRewardedInterstitialAd(int64 RequestId) = 0;
	/** Cancels only the matching App Open load identity. */
	virtual void CancelAppOpenAd(int64 RequestId) = 0;
	/** Cancels matching banner work and detaches any unfinished native view. */
	virtual void CancelBannerAd(int64 RequestId) = 0;
	/** Applies per-show verification options before presenting the loaded rewarded object. */
	virtual bool ShowRewardedAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationUserId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) = 0;
	/** Presents the interstitial object owned by the supplied completed load. */
	virtual bool ShowInterstitialAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		FString& OutError
	) = 0;
	/** Applies verification values to the loaded rewarded interstitial before presentation. */
	virtual bool ShowRewardedInterstitialAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationUserId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) = 0;
	/** Presents one loaded App Open object only for the matching show identity. */
	virtual bool ShowAppOpenAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		FString& OutError
	) = 0;
	/** Attaches and positions one loaded banner while keeping its cache reusable. */
	virtual bool ShowBannerAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FOpenMobileAdsBannerLayout& Layout,
		FString& OutError
	) = 0;
	/** Removes the matching banner view without releasing its cached native object. */
	virtual bool HideBannerAd(
		int64 LoadedRequestId,
		int64 HideRequestId,
		FString& OutError
	) = 0;
	/** Keeps the older combined rewarded request available for legacy provider callers. */
	virtual bool LaunchRewardedAd(const FString& AdUnitId, int64 RequestId, FString& OutError) = 0;
};

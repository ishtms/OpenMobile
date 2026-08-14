#pragma once

#include "IOpenMobileAdsAdMobBackend.h"

class FOpenMobileAdsAdMobIOSBackend final : public IOpenMobileAdsAdMobBackend
{
public:
	/** Keeps iOS backend identity stable for modular feature selection. */
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	/** iOS module loading already proves this Objective-C backend is the active implementation. */
	virtual bool IsAvailable() const override { return true; }
	/** Applies request and test configuration before starting Google Mobile Ads on the main queue. */
	virtual bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		int64 RequestId,
		FString& OutError
	) override;
	/** Releases delegates, views, and cached Google ad objects during provider shutdown. */
	virtual void Shutdown() override;
	/** Refreshes UMP consent information on the main queue for the active request. */
	virtual bool RequestConsentInfo(
		const FOpenMobileAdsConsentRequest& Request,
		int64 RequestId,
		FString& OutError
	) override;
	/** Presents the required UMP form from the current root view controller. */
	virtual bool PresentRequiredConsentForm(
		int64 RequestId,
		FString& OutError
	) override;
	/** Opens the user-invoked UMP privacy options form from the active view controller. */
	virtual bool PresentPrivacyOptionsForm(
		int64 RequestId,
		FString& OutError
	) override;
	/** Clears UMP test state without changing production provider settings. */
	virtual bool ResetConsentForTesting(FString& OutError) override;
	/** Applies normalized privacy flags before later Google requests are created. */
	virtual bool ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask,
		FString& OutError
	) override;
	/** Starts one rewarded load and retains its Google object only after success. */
	virtual bool LoadRewardedAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	/** Starts one interstitial load with separate native ownership. */
	virtual bool LoadInterstitialAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	/** Loads one rewarded interstitial and preserves native reward metadata. */
	virtual bool LoadRewardedInterstitialAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	/** Loads one App Open object without presenting it from UIKit lifecycle code. */
	virtual bool LoadAppOpenAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	/** Creates a Google banner using the resolved format and safe-area layout. */
	virtual bool LoadBannerAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		EOpenMobileAdFormat Format,
		const FOpenMobileAdsBannerLayout& Layout,
		FString& OutError
	) override;
	/** Cancels the matching rewarded callback ownership on the main queue. */
	virtual void CancelRewardedAd(int64 RequestId) override;
	/** Cancels only the interstitial identity supplied by the platform layer. */
	virtual void CancelInterstitialAd(int64 RequestId) override;
	/** Cancels only the rewarded-interstitial identity supplied by the platform layer. */
	virtual void CancelRewardedInterstitialAd(int64 RequestId) override;
	/** Cancels only the App Open identity supplied by the platform layer. */
	virtual void CancelAppOpenAd(int64 RequestId) override;
	/** Removes unfinished or attached banner state for the matching identity. */
	virtual void CancelBannerAd(int64 RequestId) override;
	/** Sets Google server verification options before rewarded presentation. */
	virtual bool ShowRewardedAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationUserId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) override;
	/** Presents the exact loaded interstitial from the active view controller. */
	virtual bool ShowInterstitialAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		FString& OutError
	) override;
	/** Sets verification options before presenting the loaded rewarded interstitial. */
	virtual bool ShowRewardedInterstitialAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationUserId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) override;
	/** Presents the loaded App Open object from the current root view controller. */
	virtual bool ShowAppOpenAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		FString& OutError
	) override;
	/** Attaches the loaded Google banner with current orientation and safe-area metrics. */
	virtual bool ShowBannerAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FOpenMobileAdsBannerLayout& Layout,
		FString& OutError
	) override;
	/** Removes the banner view while keeping its Google object available for reuse. */
	virtual bool HideBannerAd(
		int64 LoadedRequestId,
		int64 HideRequestId,
		FString& OutError
	) override;
	/** Keeps legacy rewarded callers on the same Objective-C backend state. */
	virtual bool LaunchRewardedAd(const FString& AdUnitId, int64 RequestId, FString& OutError) override;
};

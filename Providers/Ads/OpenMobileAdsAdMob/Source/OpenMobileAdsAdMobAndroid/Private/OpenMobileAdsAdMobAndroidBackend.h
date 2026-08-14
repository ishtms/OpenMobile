#pragma once

#include "IOpenMobileAdsAdMobBackend.h"

class FOpenMobileAdsAdMobAndroidBackend final : public IOpenMobileAdsAdMobBackend
{
public:
	/** Keeps Android's backend name stable for modular feature selection. */
	virtual FName GetBackendName() const override { return TEXT("Android"); }
	/** Android module loading already proves this JNI backend is the active platform implementation. */
	virtual bool IsAvailable() const override { return true; }
	/** Sends test devices, request policy, and privacy setup to the Java bridge before SDK startup. */
	virtual bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		int64 RequestId,
		FString& OutError
	) override;
	/** Clears Java-side objects and callbacks before the Android module unloads. */
	virtual void Shutdown() override;
	/** Starts the UMP information request through the activity-bound Java bridge. */
	virtual bool RequestConsentInfo(
		const FOpenMobileAdsConsentRequest& Request,
		int64 RequestId,
		FString& OutError
	) override;
	/** Presents the required UMP form only from the current Unreal activity. */
	virtual bool PresentRequiredConsentForm(
		int64 RequestId,
		FString& OutError
	) override;
	/** Presents Google's privacy options form from the current Android activity. */
	virtual bool PresentPrivacyOptionsForm(
		int64 RequestId,
		FString& OutError
	) override;
	/** Resets UMP test state on the Android UI thread. */
	virtual bool ResetConsentForTesting(FString& OutError) override;
	/** Applies child and under-age flags through request configuration and shared preferences where required. */
	virtual bool ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask,
		FString& OutError
	) override;
	/** Starts rewarded loading through JNI with the resolved request identity. */
	virtual bool LoadRewardedAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	/** Starts an independent Java interstitial load for the supplied identity. */
	virtual bool LoadInterstitialAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	/** Starts rewarded-interstitial loading and keeps reward metadata native-side. */
	virtual bool LoadRewardedInterstitialAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	/** Starts an App Open load without coupling it to Android activity callbacks. */
	virtual bool LoadAppOpenAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	/** Resolves banner format and layout into the Java view request. */
	virtual bool LoadBannerAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		EOpenMobileAdFormat Format,
		const FOpenMobileAdsBannerLayout& Layout,
		FString& OutError
	) override;
	/** Cancels the matching Java rewarded load or queued callback. */
	virtual void CancelRewardedAd(int64 RequestId) override;
	/** Cancels only the Java interstitial work with this identity. */
	virtual void CancelInterstitialAd(int64 RequestId) override;
	/** Cancels only the Java rewarded-interstitial work with this identity. */
	virtual void CancelRewardedInterstitialAd(int64 RequestId) override;
	/** Cancels only the Java App Open work with this identity. */
	virtual void CancelAppOpenAd(int64 RequestId) override;
	/** Removes unfinished or visible banner work owned by this identity. */
	virtual void CancelBannerAd(int64 RequestId) override;
	/** Sets rewarded verification options before asking the Java bridge to present. */
	virtual bool ShowRewardedAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationUserId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) override;
	/** Presents the exact Java interstitial object created by the loaded identity. */
	virtual bool ShowInterstitialAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		FString& OutError
	) override;
	/** Applies verification values before showing the loaded rewarded interstitial. */
	virtual bool ShowRewardedInterstitialAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationUserId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) override;
	/** Presents the loaded App Open object through the current activity. */
	virtual bool ShowAppOpenAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		FString& OutError
	) override;
	/** Attaches the loaded banner view with the latest resolved placement layout. */
	virtual bool ShowBannerAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FOpenMobileAdsBannerLayout& Layout,
		FString& OutError
	) override;
	/** Detaches the banner view while preserving its loaded Java object. */
	virtual bool HideBannerAd(
		int64 LoadedRequestId,
		int64 HideRequestId,
		FString& OutError
	) override;
	/** Runs the legacy rewarded request through the same Java bridge state. */
	virtual bool LaunchRewardedAd(const FString& AdUnitId, int64 RequestId, FString& OutError) override;
};

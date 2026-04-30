#pragma once

#include "IOpenMobileAdsAdMobBackend.h"

class FOpenMobileAdsAdMobIOSBackend final : public IOpenMobileAdsAdMobBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	virtual bool IsAvailable() const override { return true; }
	virtual bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		int64 RequestId,
		FString& OutError
	) override;
	virtual void Shutdown() override;
	virtual bool RequestConsentInfo(
		const FOpenMobileAdsConsentRequest& Request,
		int64 RequestId,
		FString& OutError
	) override;
	virtual bool PresentRequiredConsentForm(
		int64 RequestId,
		FString& OutError
	) override;
	virtual bool PresentPrivacyOptionsForm(
		int64 RequestId,
		FString& OutError
	) override;
	virtual bool ResetConsentForTesting(FString& OutError) override;
	virtual bool ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask,
		FString& OutError
	) override;
	virtual bool LoadRewardedAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	virtual bool LoadInterstitialAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	virtual bool LoadBannerAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		EOpenMobileAdFormat Format,
		const FOpenMobileAdsBannerLayout& Layout,
		FString& OutError
	) override;
	virtual void CancelRewardedAd(int64 RequestId) override;
	virtual void CancelInterstitialAd(int64 RequestId) override;
	virtual void CancelBannerAd(int64 RequestId) override;
	virtual bool ShowRewardedAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) override;
	virtual bool ShowInterstitialAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		FString& OutError
	) override;
	virtual bool ShowBannerAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FOpenMobileAdsBannerLayout& Layout,
		FString& OutError
	) override;
	virtual bool HideBannerAd(
		int64 LoadedRequestId,
		int64 HideRequestId,
		FString& OutError
	) override;
	virtual bool LaunchRewardedAd(const FString& AdUnitId, int64 RequestId, FString& OutError) override;
};

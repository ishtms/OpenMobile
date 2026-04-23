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
	virtual bool LoadRewardedAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) override;
	virtual void CancelRewardedAd(int64 RequestId) override;
	virtual bool ShowRewardedAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) override;
	virtual bool LaunchRewardedAd(const FString& AdUnitId, int64 RequestId, FString& OutError) override;
};

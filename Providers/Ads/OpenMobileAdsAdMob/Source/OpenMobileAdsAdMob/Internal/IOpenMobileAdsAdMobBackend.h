#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileAdsOperations.h"

/** Native AdMob SDK boundary, internal to the AdMob provider plugin. */
class IOpenMobileAdsAdMobBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileAdsAdMobBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Ads.AdMob.Backend"));
		return FeatureName;
	}

	virtual FName GetBackendName() const = 0;
	virtual bool IsAvailable() const = 0;
	virtual bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		int64 RequestId,
		FString& OutError
	) = 0;
	virtual void Shutdown() = 0;
	virtual bool RequestConsentInfo(
		const FOpenMobileAdsConsentRequest& Request,
		int64 RequestId,
		FString& OutError
	) = 0;
	virtual bool PresentRequiredConsentForm(
		int64 RequestId,
		FString& OutError
	) = 0;
	virtual bool PresentPrivacyOptionsForm(
		int64 RequestId,
		FString& OutError
	) = 0;
	virtual bool LoadRewardedAd(
		const FString& AdUnitId,
		int64 RequestId,
		EOpenMobileAdsDataProcessingMode DataProcessingMode,
		FString& OutError
	) = 0;
	virtual void CancelRewardedAd(int64 RequestId) = 0;
	virtual bool ShowRewardedAd(
		int64 LoadedRequestId,
		int64 ShowRequestId,
		const FString& ServerVerificationCustomData,
		FString& OutError
	) = 0;
	virtual bool LaunchRewardedAd(const FString& AdUnitId, int64 RequestId, FString& OutError) = 0;
};

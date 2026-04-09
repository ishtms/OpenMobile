#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

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
	virtual void Initialize() = 0;
	virtual void Shutdown() = 0;
	virtual bool LaunchRewardedAd(const FString& AdUnitId, int64 RequestId, FString& OutError) = 0;
};

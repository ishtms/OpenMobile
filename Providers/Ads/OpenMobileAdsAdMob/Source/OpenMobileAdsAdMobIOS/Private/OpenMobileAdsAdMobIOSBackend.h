#pragma once

#include "IOpenMobileAdsAdMobBackend.h"

class FOpenMobileAdsAdMobIOSBackend final : public IOpenMobileAdsAdMobBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	virtual bool IsAvailable() const override { return true; }
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual bool LaunchRewardedAd(const FString& AdUnitId, int64 RequestId, FString& OutError) override;
};

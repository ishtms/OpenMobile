#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

class FOpenMobileAdsAdMobUnityConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	virtual FName GetConsumerName() const override
	{
		return TEXT("Unity Ads");
	}

	virtual int32 GetSupportedConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return 0;
	}

	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

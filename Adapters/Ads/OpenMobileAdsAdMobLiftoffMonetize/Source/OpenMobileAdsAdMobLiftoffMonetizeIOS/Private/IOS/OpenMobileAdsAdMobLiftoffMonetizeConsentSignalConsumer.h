#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

class FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer final
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
		return TEXT("Liftoff Monetize");
	}

	virtual int32 GetSupportedConsentSignalMask() const override;

	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return 0;
	}

	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override;

	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

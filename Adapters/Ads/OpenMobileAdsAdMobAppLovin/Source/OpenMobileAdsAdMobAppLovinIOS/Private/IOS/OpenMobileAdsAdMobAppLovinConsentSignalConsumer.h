#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

class FOpenMobileAdsAdMobAppLovinConsentSignalConsumer final
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
		return TEXT("AppLovin");
	}

	virtual int32 GetSupportedConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
	{
		return static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr)
			| static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	}

	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

/** Delivers consent state to Chartboost before its Android AdMob adapter can request inventory. */
class FOpenMobileAdsAdMobChartboostConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	/** Holds Chartboost under the AdMob provider's privacy startup checks. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Labels Chartboost as a mediated network in delivery status. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	/** Uses Chartboost's stable mediation-facing network name. */
	virtual FName GetConsumerName() const override
	{
		return TEXT("Chartboost");
	}

	/** Accepts the full normalized signal set before Android adapter initialization. */
	virtual int32 GetSupportedConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Confirms every signal after the Chartboost Java bridge accepts it. */
	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Restricts live changes to Chartboost GDPR and US privacy controls. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
	{
		return static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr)
			| static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	}

	/** Converts normalized choices to Chartboost values and returns a typed bridge failure. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

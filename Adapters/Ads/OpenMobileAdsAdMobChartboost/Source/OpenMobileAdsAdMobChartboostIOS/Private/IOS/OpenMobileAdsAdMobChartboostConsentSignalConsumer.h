#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

/** Applies consent state to the Chartboost iOS SDK before AdMob mediation initializes it. */
class FOpenMobileAdsAdMobChartboostConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	/** Makes Chartboost privacy delivery part of AdMob startup. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Keeps Chartboost in the mediated network status group. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	/** Returns the Chartboost name used in provider diagnostics. */
	virtual FName GetConsumerName() const override
	{
		return TEXT("Chartboost");
	}

	/** Accepts all normalized privacy values before the native SDK starts. */
	virtual int32 GetSupportedConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Confirms each value after Chartboost's iOS privacy API accepts it. */
	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Allows live GDPR and US privacy changes supported by Chartboost. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
	{
		return static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr)
			| static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	}

	/** Applies selected values on the main queue and reports any native refusal. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

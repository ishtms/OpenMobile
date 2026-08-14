#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

/** Delivers normalized privacy state to AppLovin before AdMob mediation can initialize it on Android. */
class FOpenMobileAdsAdMobAppLovinConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	/** Ties AppLovin signal delivery to the AdMob provider's startup gate. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Reports AppLovin as a mediated network in delivery diagnostics. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	/** Uses the network name shown in AdMob mediation status. */
	virtual FName GetConsumerName() const override
	{
		return TEXT("AppLovin");
	}

	/** Accepts GDPR, US privacy, child-directed, and under-age values before SDK startup. */
	virtual int32 GetSupportedConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Confirms every Android AppLovin value after the Java bridge applies it. */
	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Limits runtime changes to AppLovin's GDPR and do-not-sell APIs. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
	{
		return static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr)
			| static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	}

	/** Sends only required values through JNI and reports bridge failure to provider initialization. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

/** Delivers normalized privacy state to Unity Ads before Android mediation startup. */
class FOpenMobileAdsAdMobUnityConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	/** Keeps Unity Ads under AdMob's provider-level privacy gate. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Labels Unity Ads as a mediated network in signal delivery rows. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	/** Returns the network name used by Google's Unity adapter. */
	virtual FName GetConsumerName() const override
	{
		return TEXT("Unity Ads");
	}

	/** Accepts every normalized signal exposed by the Unity Android privacy bridge. */
	virtual int32 GetSupportedConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Confirms all values after the Java metadata calls succeed. */
	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Allows the Unity bridge to receive the complete signal set again at runtime. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Converts each configured value to Unity metadata and reports Java bridge failure. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

/** Delivers privacy state to Liftoff Monetize before its iOS AdMob adapter initializes. */
class FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	/** Ties Liftoff's native setup to the AdMob provider generation. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Keeps Liftoff in the mediated network section of privacy diagnostics. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	/** Matches the network name used by Google's Liftoff adapter. */
	virtual FName GetConsumerName() const override
	{
		return TEXT("Liftoff Monetize");
	}

	/** Declares the privacy signals supported by the packaged iOS Liftoff SDK. */
	virtual int32 GetSupportedConsentSignalMask() const override;

	/** Leaves confirmation empty because this SDK version has no reliable privacy getters. */
	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return 0;
	}

	/** Declares only the values safe to apply again after SDK startup. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override;

	/** Applies supported Liftoff values on the main queue without claiming unobservable confirmation. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

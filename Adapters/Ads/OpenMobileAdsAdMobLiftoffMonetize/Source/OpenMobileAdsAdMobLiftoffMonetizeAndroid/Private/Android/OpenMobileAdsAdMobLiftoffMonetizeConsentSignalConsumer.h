#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

/** Sends required privacy state to Liftoff Monetize before its Android AdMob adapter starts. */
class FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	/** Makes Liftoff signal delivery a prerequisite for AdMob initialization. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Reports Liftoff as a mediated network in consent delivery status. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	/** Uses the full network name expected by adapter diagnostics. */
	virtual FName GetConsumerName() const override
	{
		return TEXT("Liftoff Monetize");
	}

	/** Declares only the privacy values supported by this Android Liftoff SDK version. */
	virtual int32 GetSupportedConsentSignalMask() const override;

	/** Reports which Java-applied values can be confirmed after delivery. */
	virtual int32 GetConfirmableConsentSignalMask() const override;

	/** Limits runtime delivery to values Liftoff accepts after initialization. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override;

	/** Converts normalized signals to Liftoff's Java privacy calls with typed failure reporting. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

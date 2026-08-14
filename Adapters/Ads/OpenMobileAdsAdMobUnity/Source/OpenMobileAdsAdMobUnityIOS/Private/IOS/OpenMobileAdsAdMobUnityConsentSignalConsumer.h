#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

/** Applies normalized privacy state to Unity Ads before iOS mediation startup. */
class FOpenMobileAdsAdMobUnityConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	/** Makes Unity privacy delivery part of AdMob's initialization checks. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Groups Unity Ads with mediated networks in diagnostics. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	/** Uses the Unity Ads name shown in mediation status. */
	virtual FName GetConsumerName() const override
	{
		return TEXT("Unity Ads");
	}

	/** Accepts every privacy value supported by Unity's metadata API. */
	virtual int32 GetSupportedConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Leaves confirmation empty because Unity metadata offers no dependable readback. */
	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return 0;
	}

	/** Allows the full metadata set to be refreshed while the SDK is running. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Writes selected metadata on the main queue without inventing confirmation. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

#pragma once

#include "IOpenMobileAdsConsentSignalConsumer.h"

/** Applies normalized privacy state to AppLovin before AdMob mediation starts it on iOS. */
class FOpenMobileAdsAdMobAppLovinConsentSignalConsumer final
	: public IOpenMobileAdsConsentSignalConsumer
{
public:
	/** Makes this consumer part of AdMob's initialization requirements. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Keeps AppLovin grouped with mediated networks rather than the provider itself. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
	{
		return EOpenMobileAdsConsentSignalConsumerType::Network;
	}

	/** Matches the AppLovin network name used in mediation reports. */
	virtual FName GetConsumerName() const override
	{
		return TEXT("AppLovin");
	}

	/** Accepts every normalized signal before the AppLovin SDK is initialized. */
	virtual int32 GetSupportedConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Confirms each iOS AppLovin setting by reading it back from ALPrivacySettings. */
	virtual int32 GetConfirmableConsentSignalMask() const override
	{
		return FOpenMobileAdsConsentSignals::AllSignalMask;
	}

	/** Allows live GDPR and do-not-sell updates supported by AppLovin. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
	{
		return static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr)
			| static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	}

	/** Runs AppLovin privacy setters on the main queue and verifies their stored values. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) override;
};

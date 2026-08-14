#pragma once

#include "Features/IModularFeature.h"
#include "OpenMobileAdsPrivacy.h"

/** Lets optional provider adapters receive privacy signals without coupling them to the core Ads module. */
class OPENMOBILEADS_API IOpenMobileAdsConsentSignalConsumer
	: public IModularFeature
{
public:
	virtual ~IOpenMobileAdsConsentSignalConsumer() = default;

	/** Uses one stable modular feature key for every provider and mediation consumer. */
	static FName GetModularFeatureName()
	{
		static const FName FeatureName(
			TEXT("OpenMobile.Ads.ConsentSignalConsumer")
		);
		return FeatureName;
	}

	/** Names the provider whose initialization must wait for this consumer when signals are required. */
	virtual FName GetOwningProviderName() const = 0;
	/** Separates provider, network, and adapter status rows in diagnostics. */
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const = 0;
	/** Returns the stable consumer name used to merge delivery status across updates. */
	virtual FName GetConsumerName() const = 0;
	/** Links adapter or network status to its parent when the SDK reports a hierarchy. */
	virtual FName GetParentName() const { return NAME_None; }
	/** Declares every privacy signal this SDK can accept before filtering starts. */
	virtual int32 GetSupportedConsentSignalMask() const = 0;
	/** Declares which applied signals the SDK can confirm instead of merely accepting. */
	virtual int32 GetConfirmableConsentSignalMask() const { return 0; }
	/** Declares which signals can change safely after provider initialization. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const { return 0; }
	/** Applies only the requested supported bits and reports acceptance separately from confirmation. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) = 0;
};

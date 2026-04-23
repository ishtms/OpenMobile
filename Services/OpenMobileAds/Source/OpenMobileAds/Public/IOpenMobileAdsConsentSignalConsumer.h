#pragma once

#include "Features/IModularFeature.h"
#include "OpenMobileAdsPrivacy.h"

class OPENMOBILEADS_API IOpenMobileAdsConsentSignalConsumer
	: public IModularFeature
{
public:
	virtual ~IOpenMobileAdsConsentSignalConsumer() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(
			TEXT("OpenMobile.Ads.ConsentSignalConsumer")
		);
		return FeatureName;
	}

	virtual FName GetOwningProviderName() const = 0;
	virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const = 0;
	virtual FName GetConsumerName() const = 0;
	virtual FName GetParentName() const { return NAME_None; }
	virtual int32 GetSupportedConsentSignalMask() const = 0;
	virtual int32 GetConfirmableConsentSignalMask() const { return 0; }
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const { return 0; }
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	) = 0;
};

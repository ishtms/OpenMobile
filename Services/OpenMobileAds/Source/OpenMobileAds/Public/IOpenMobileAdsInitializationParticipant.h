#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

struct FOpenMobileAdsError;
struct FOpenMobileAdsInitializationRequest;

class OPENMOBILEADS_API IOpenMobileAdsInitializationParticipant
	: public IModularFeature
{
public:
	virtual ~IOpenMobileAdsInitializationParticipant() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(
			TEXT("OpenMobile.Ads.InitializationParticipant")
		);
		return FeatureName;
	}

	virtual FName GetOwningProviderName() const = 0;
	virtual FName GetParticipantName() const = 0;
	virtual bool PrepareForInitialization(
		const FOpenMobileAdsInitializationRequest& Request,
		FOpenMobileAdsError& OutError
	) = 0;
};

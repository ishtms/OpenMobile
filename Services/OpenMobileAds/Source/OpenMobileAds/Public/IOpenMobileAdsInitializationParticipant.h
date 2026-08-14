#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

struct FOpenMobileAdsError;
struct FOpenMobileAdsInitializationRequest;

/** Lets optional adapters prepare native SDK state before their owning provider initializes. */
class OPENMOBILEADS_API IOpenMobileAdsInitializationParticipant
	: public IModularFeature
{
public:
	virtual ~IOpenMobileAdsInitializationParticipant() = default;

	/** Uses one stable modular feature key so providers can discover optional participants. */
	static FName GetModularFeatureName()
	{
		static const FName FeatureName(
			TEXT("OpenMobile.Ads.InitializationParticipant")
		);
		return FeatureName;
	}

	/** Names the provider whose initialization this participant must precede. */
	virtual FName GetOwningProviderName() const = 0;
	/** Returns the stable row name used in initialization diagnostics. */
	virtual FName GetParticipantName() const = 0;
	/** Applies required native setup synchronously and blocks provider startup on a real failure. */
	virtual bool PrepareForInitialization(
		const FOpenMobileAdsInitializationRequest& Request,
		FOpenMobileAdsError& OutError
	) = 0;
};

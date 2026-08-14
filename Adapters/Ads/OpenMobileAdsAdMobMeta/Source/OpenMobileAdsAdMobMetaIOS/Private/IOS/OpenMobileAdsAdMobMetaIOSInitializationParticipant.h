#pragma once

#include "IOpenMobileAdsInitializationParticipant.h"

/** Applies Meta's legacy iOS tracking flag before AdMob initializes the Audience Network adapter. */
class FOpenMobileAdsAdMobMetaIOSInitializationParticipant final
	: public IOpenMobileAdsInitializationParticipant
{
public:
	/** Runs this participant only for the AdMob provider generation. */
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	/** Uses a stable participant name so duplicate Meta setup is rejected. */
	virtual FName GetParticipantName() const override
	{
		return TEXT("MetaAudienceNetwork");
	}

	/** Applies the pre-iOS 17 advertiser tracking flag on the main queue before SDK startup. */
	virtual bool PrepareForInitialization(
		const FOpenMobileAdsInitializationRequest& Request,
		FOpenMobileAdsError& OutError
	) override;
};

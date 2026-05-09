#pragma once

#include "IOpenMobileAdsInitializationParticipant.h"

class FOpenMobileAdsAdMobMetaIOSInitializationParticipant final
	: public IOpenMobileAdsInitializationParticipant
{
public:
	virtual FName GetOwningProviderName() const override
	{
		return TEXT("AdMob");
	}

	virtual FName GetParticipantName() const override
	{
		return TEXT("MetaAudienceNetwork");
	}

	virtual bool PrepareForInitialization(
		const FOpenMobileAdsInitializationRequest& Request,
		FOpenMobileAdsError& OutError
	) override;
};

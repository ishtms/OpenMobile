#pragma once

#include "OpenMobileAdsPrivacy.h"

enum class EOpenMobileAdsAdMobUMPConsentStatus : uint8
{
	Unknown,
	NotRequired,
	Required,
	Obtained
};

class FOpenMobileAdsAdMobConsentMapper
{
public:
	static FOpenMobileAdsConsentStatusUpdate MapGdprState(
		EOpenMobileAdsAdMobUMPConsentStatus Status,
		bool bCanRequestAds,
		FName Source
	);
};

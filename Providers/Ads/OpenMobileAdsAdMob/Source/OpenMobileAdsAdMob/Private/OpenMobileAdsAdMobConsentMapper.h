#pragma once

#include "OpenMobileAdsPrivacy.h"

enum class EOpenMobileAdsAdMobUMPConsentStatus : uint8
{
	Unknown,
	NotRequired,
	Required,
	Obtained
};

enum class EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement : uint8
{
	Unknown,
	NotRequired,
	Required
};

class FOpenMobileAdsAdMobConsentMapper
{
public:
	static FOpenMobileAdsConsentStatusUpdate MapGdprState(
		EOpenMobileAdsAdMobUMPConsentStatus Status,
		bool bCanRequestAds,
		FName Source,
		EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement PrivacyOptions =
			EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Unknown
	);
	static FOpenMobileAdsUsPrivacyState MapUsPrivacyState(
		EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement Requirement
	);
};

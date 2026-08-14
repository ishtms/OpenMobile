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
	/** Combines UMP status, request permission, and privacy-options availability into one normalized update. */
	static FOpenMobileAdsConsentStatusUpdate MapGdprState(
		EOpenMobileAdsAdMobUMPConsentStatus Status,
		bool bCanRequestAds,
		FName Source,
		EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement PrivacyOptions =
			EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Unknown
	);
	/** Maps UMP privacy-options availability without inventing a US opt-out choice. */
	static FOpenMobileAdsUsPrivacyState MapUsPrivacyState(
		EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement Requirement
	);
};

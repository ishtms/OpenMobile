#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsPrivacy.h"
#include "OpenMobileAdsTypes.h"

struct FOpenMobileAdsCanRequestAdsContext
{
	EOpenMobileAdsServiceState ServiceState =
		EOpenMobileAdsServiceState::Uninitialized;
	bool bProviderAvailable = false;
	FName Provider;
	EOpenMobileAdsConsentStatus ConsentStatus =
		EOpenMobileAdsConsentStatus::Unknown;
	EOpenMobileAdsConsentActivity ConsentActivity =
		EOpenMobileAdsConsentActivity::Idle;
	EOpenMobileAdsConsentRequestState ConsentRequestState =
		EOpenMobileAdsConsentRequestState::Unknown;
	FOpenMobileAdsUsPrivacyState UsPrivacy;
	bool bConsentStatusFresh = false;
	FOpenMobileAdsProviderRequestPolicy ProviderPolicy;
};

class FOpenMobileAdsCanRequestPolicy
{
public:
	static FOpenMobileAdsCanRequestAdsResult Evaluate(
		const FOpenMobileAdsCanRequestAdsContext& Context
	);
};

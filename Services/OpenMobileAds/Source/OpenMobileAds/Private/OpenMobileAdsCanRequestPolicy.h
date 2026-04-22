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

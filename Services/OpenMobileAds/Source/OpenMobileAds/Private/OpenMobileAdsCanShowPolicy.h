#pragma once

#include "OpenMobileAdsResults.h"

struct FOpenMobileAdsCanShowPolicyContext
{
	bool bPlacementConfigured = false;
	bool bPlacementEnabled = false;
	EOpenMobileAdsServiceState ServiceState = EOpenMobileAdsServiceState::Uninitialized;
	bool bProviderAvailable = false;
	bool bFormatSupported = false;
	bool bPrivacyAllowed = false;
	EOpenMobileAdPlacementState PlacementState = EOpenMobileAdPlacementState::Idle;
	bool bHasCachedAd = false;
	bool bExpired = false;
	bool bFrequencyCapped = false;
	bool bCooldownActive = false;
	bool bOffline = false;
	bool bLifecycleConflict = false;
	EOpenMobileAdsFrequencyCapScope FrequencyCapScope =
		EOpenMobileAdsFrequencyCapScope::None;
	FDateTime FrequencyCapEndsAt;
	FDateTime CooldownEndsAt;
	FString ServiceExplanation;
	FString ProviderExplanation;
};

class FOpenMobileAdsCanShowPolicy
{
public:
	/** Returns the first placement, cache, pacing, network, or lifecycle gate in stable order. */
	static FOpenMobileAdsCanShowResult Evaluate(
		const FOpenMobileAdsCanShowPolicyContext& Context
	);
};

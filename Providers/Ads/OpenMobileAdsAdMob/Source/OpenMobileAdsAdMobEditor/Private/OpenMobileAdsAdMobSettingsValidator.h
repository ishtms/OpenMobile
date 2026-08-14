#pragma once

#include "CoreMinimal.h"

class UOpenMobileAdsAdMobSettings;

class FOpenMobileAdsAdMobSettingsValidator
{
public:
	/** Checks provider identifiers and shipping safety without loading a native AdMob SDK. */
	static TArray<FString> Validate(
		const UOpenMobileAdsAdMobSettings& Settings,
		bool bForShipping = false
	);
};

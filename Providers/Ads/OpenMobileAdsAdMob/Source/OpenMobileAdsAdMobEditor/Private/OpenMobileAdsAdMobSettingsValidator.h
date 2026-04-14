#pragma once

#include "CoreMinimal.h"

class UOpenMobileAdsAdMobSettings;

class FOpenMobileAdsAdMobSettingsValidator
{
public:
	static TArray<FString> Validate(
		const UOpenMobileAdsAdMobSettings& Settings,
		bool bForShipping = false
	);
};

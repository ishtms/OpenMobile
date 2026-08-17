#pragma once

#include "CoreMinimal.h"

class UOpenMobileAdsAdMobSettings;
enum class EOpenMobileAdsPlatform : uint8;

class FOpenMobileAdsAdMobSettingsValidator
{
public:
	/** Checks provider identifiers and shipping safety without loading a native AdMob SDK. */
	static TArray<FString> Validate(
		const UOpenMobileAdsAdMobSettings& Settings,
		bool bForShipping = false
	);

	static TArray<FString> ValidateForPlatforms(
		const UOpenMobileAdsAdMobSettings& Settings,
		const TArray<EOpenMobileAdsPlatform>& Platforms,
		bool bForShipping = false
	);
};

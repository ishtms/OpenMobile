#pragma once

#include "CoreMinimal.h"

class FOpenMobileDeviceNativeConfigurationPolicy final
{
public:
	static bool IsValidUrlScheme(const FString& Value);
	static bool TryNormalizeUrlScheme(
		const FString& Value,
		FString& OutNormalized
	);
	static bool IsValidAndroidIntentAction(const FString& Value);
	static bool IsValidAndroidPackage(const FString& Value);
};

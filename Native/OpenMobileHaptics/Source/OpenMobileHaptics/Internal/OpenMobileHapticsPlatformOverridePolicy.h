#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPlatformAssets.h"

class UOpenMobileHapticPatternAsset;

enum class EOpenMobileHapticsPlatformOverridePath : uint8
{
	ExactOverride,
	PortablePattern,
	NoEffect,
	Rejected
};

struct FOpenMobileHapticsPlatformOverrideResolution
{
	EOpenMobileHapticsPlatformOverridePath Path =
		EOpenMobileHapticsPlatformOverridePath::Rejected;
	FSoftObjectPath OverrideAsset;
	FName Reason;
};

class FOpenMobileHapticsPlatformOverridePolicy final
{
public:
	static FOpenMobileHapticsPlatformOverrideResolution Resolve(
		const UOpenMobileHapticPatternAsset& Pattern,
		EOpenMobileHapticOverridePlatform Platform,
		int32 OSVersion,
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
};

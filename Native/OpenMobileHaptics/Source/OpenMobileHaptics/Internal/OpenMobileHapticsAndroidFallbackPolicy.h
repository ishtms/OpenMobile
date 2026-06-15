#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPatternAsset.h"

enum class EOpenMobileHapticsAndroidFallbackOutcome : uint8
{
	Primitive,
	NoEffect,
	Rejected
};

struct FOpenMobileHapticsAndroidFallbackResolution
{
	EOpenMobileHapticsAndroidFallbackOutcome Outcome =
		EOpenMobileHapticsAndroidFallbackOutcome::Rejected;
	EOpenMobileHapticAndroidPrimitive Primitive =
		EOpenMobileHapticAndroidPrimitive::Click;
	TArray<FName> Attempts;
};

class FOpenMobileHapticsAndroidFallbackPolicy final
{
public:
	static FOpenMobileHapticsAndroidFallbackResolution ResolvePrimitive(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy RequestPolicy
	);
};

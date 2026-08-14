#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPlatformAssets.h"

enum class EOpenMobileHapticsPrimitiveCompositionOutcome : uint8
{
	Ready,
	FallbackRequired,
	Rejected
};

struct FOpenMobileHapticsPrimitiveCompositionResolution
{
	EOpenMobileHapticsPrimitiveCompositionOutcome Outcome =
		EOpenMobileHapticsPrimitiveCompositionOutcome::Rejected;
	FName Reason;
	TArray<EOpenMobileHapticAndroidPrimitive> Primitives;
	TArray<float> Scales;
	TArray<int32> DelaysMilliseconds;
};

class FOpenMobileHapticsPrimitiveCompositionPolicy final
{
public:
	static constexpr int32 MaximumStepDelayMilliseconds = 10000;
	static constexpr int64 MaximumTotalDelayMilliseconds = 30000;

	/** Validates primitive composition against Android version and device support before the bridge allocates native arrays. */
	static FOpenMobileHapticsPrimitiveCompositionResolution Resolve(
		const UOpenMobileHapticAndroidPatternAsset& Asset,
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 AndroidAPI,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
};

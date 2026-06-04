#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsOneShotPath : uint8
{
	Unsupported,
	SystemSemantic,
	PredefinedEffect,
	BasicVibration
};

struct FOpenMobileHapticsOneShotResolution
{
	EOpenMobileHapticsOneShotPath Path =
		EOpenMobileHapticsOneShotPath::Unsupported;
	bool bSuppressWhenUnavailable = false;
};

class FOpenMobileHapticsOneShotPolicy final
{
public:
	static FOpenMobileHapticsOneShotResolution Resolve(
		const FOpenMobileHapticCapabilities& Capabilities,
		double DurationSeconds,
		EOpenMobileHapticFallbackPolicy FallbackPolicy =
			EOpenMobileHapticFallbackPolicy::Automatic
	);
	static FName PathName(EOpenMobileHapticsOneShotPath Path);
};

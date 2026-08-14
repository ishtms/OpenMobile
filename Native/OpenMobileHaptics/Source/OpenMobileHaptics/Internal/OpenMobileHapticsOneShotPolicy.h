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
	/** Picks the best one-shot route the current device can honour, including the caller's no-effect fallback choice. */
	static FOpenMobileHapticsOneShotResolution Resolve(
		const FOpenMobileHapticCapabilities& Capabilities,
		double DurationSeconds,
		EOpenMobileHapticFallbackPolicy FallbackPolicy =
			EOpenMobileHapticFallbackPolicy::Automatic
	);
	/** Returns a stable diagnostic name instead of leaking the internal path enum into logs. */
	static FName PathName(EOpenMobileHapticsOneShotPath Path);
};

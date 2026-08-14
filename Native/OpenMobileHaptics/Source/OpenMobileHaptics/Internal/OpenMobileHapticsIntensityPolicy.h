#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsIntensityOutcome : uint8
{
	Accepted,
	DefaultAmplitudeFallback,
	Suppressed,
	Rejected
};

struct FOpenMobileHapticsIntensityResolution
{
	EOpenMobileHapticsIntensityOutcome Outcome =
		EOpenMobileHapticsIntensityOutcome::Rejected;
	float ResolvedIntensity = 0.0f;
	bool bNativeIntensityKnown = false;
	float NativeIntensity = 0.0f;
	bool bNativeClamped = false;
};

class FOpenMobileHapticsIntensityPolicy final
{
public:
	/** Multiplies every intensity layer in one place, then clamps once so callers don't produce different answers from the same settings. */
	static float Scale(
		float BaseIntensity,
		float MasterScale,
		float CategoryScale,
		float EffectScale,
		float RequestScale,
		float ProjectScale
	);
	/** Decides whether a basic vibration can honour amplitude, should use the device default, or must follow fallback policy. */
	static FOpenMobileHapticsIntensityResolution ResolveBasicVibration(
		float Intensity,
		EOpenMobileHapticSupportState AmplitudeControl,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
};

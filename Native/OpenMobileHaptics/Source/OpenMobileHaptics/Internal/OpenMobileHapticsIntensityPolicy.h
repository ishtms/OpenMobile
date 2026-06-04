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
	static float Scale(
		float BaseIntensity,
		float MasterScale,
		float CategoryScale,
		float EffectScale,
		float RequestScale,
		float ProjectScale
	);
	static FOpenMobileHapticsIntensityResolution ResolveBasicVibration(
		float Intensity,
		EOpenMobileHapticSupportState AmplitudeControl,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
};

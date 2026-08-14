#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPlatformAssets.h"

enum class EOpenMobileHapticsEnvelopeOutcome : uint8
{
	Ready,
	FallbackRequired,
	Rejected
};

struct FOpenMobileHapticsEnvelopeResolution
{
	EOpenMobileHapticsEnvelopeOutcome Outcome =
		EOpenMobileHapticsEnvelopeOutcome::Rejected;
	EOpenMobileHapticAndroidPatternFormat Format =
		EOpenMobileHapticAndroidPatternFormat::BasicEnvelope;
	FName Reason;
	TArray<float> Amplitudes;
	TArray<float> ControlValues;
	TArray<int64> DurationsMilliseconds;
};

class FOpenMobileHapticsEnvelopePolicy final
{
public:
	/** Validates Android envelope data and picks the format the current API can really play, with fallback left to the caller. */
	static FOpenMobileHapticsEnvelopeResolution Resolve(
		const UOpenMobileHapticAndroidPatternAsset& Asset,
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 AndroidAPI,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
};

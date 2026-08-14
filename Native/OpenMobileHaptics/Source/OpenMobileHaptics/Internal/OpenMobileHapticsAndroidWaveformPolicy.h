#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

class UOpenMobileHapticAndroidPatternAsset;
class UOpenMobileHapticPatternAsset;

enum class EOpenMobileHapticsAndroidWaveformOutcome : uint8
{
	Ready,
	FallbackRequired,
	Rejected
};

struct FOpenMobileHapticsAndroidWaveformResolution
{
	EOpenMobileHapticsAndroidWaveformOutcome Outcome =
		EOpenMobileHapticsAndroidWaveformOutcome::Rejected;
	TArray<int64> TimingsMilliseconds;
	TArray<int32> Amplitudes;
	int32 RepeatIndex = INDEX_NONE;
	FName Reason;
	bool bUsesDefaultAmplitude = false;
};

class FOpenMobileHapticsAndroidWaveformPolicy final
{
public:
	/** Validates an Android override against the current API and amplitude support before native playback sees it. */
	static FOpenMobileHapticsAndroidWaveformResolution ResolveOverride(
		const UOpenMobileHapticAndroidPatternAsset& Asset,
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 AndroidAPI,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
	/** Compiles the portable pattern once with its own loop data, handy for ordinary playback requests. */
	static FOpenMobileHapticsAndroidWaveformResolution ResolvePortable(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
	/** Compiles with caller-supplied looping so pause, resume, and request overrides all agree on the same waveform. */
	static FOpenMobileHapticsAndroidWaveformResolution ResolvePortable(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticLoopOptions& Loop,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
};

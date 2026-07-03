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
	static FOpenMobileHapticsAndroidWaveformResolution ResolveOverride(
		const UOpenMobileHapticAndroidPatternAsset& Asset,
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 AndroidAPI,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
	static FOpenMobileHapticsAndroidWaveformResolution ResolvePortable(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
	static FOpenMobileHapticsAndroidWaveformResolution ResolvePortable(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticLoopOptions& Loop,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
};

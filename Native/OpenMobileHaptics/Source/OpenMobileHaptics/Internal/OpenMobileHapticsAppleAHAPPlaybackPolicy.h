#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsAppleBridgeService.h"
#include "OpenMobileHapticsTypes.h"

class UOpenMobileHapticIOSPatternAsset;
class UOpenMobileHapticsSettings;

enum class EOpenMobileHapticsAppleAHAPOutcome : uint8
{
	Ready,
	FallbackRequired,
	Invalid
};

struct FOpenMobileHapticsAppleAHAPLimits
{
	int32 MaximumFiniteRepeatCount = 32;
	double MaximumDurationSeconds = 300.0;
	double MinimumCompletionDurationSeconds = 0.001;
};

struct FOpenMobileHapticsAppleAHAPResolution
{
	EOpenMobileHapticsAppleAHAPOutcome Outcome =
		EOpenMobileHapticsAppleAHAPOutcome::Invalid;
	FOpenMobileHapticsAppleAHAPPattern Pattern;
	FName Reason;
};

class FOpenMobileHapticsAppleAHAPPlaybackPolicy final
{
public:
	static FOpenMobileHapticsAppleAHAPLimits MakeLimits(
		const UOpenMobileHapticsSettings& Settings
	);

	static FOpenMobileHapticsAppleAHAPResolution Resolve(
		const UOpenMobileHapticIOSPatternAsset& Asset,
		const FOpenMobileHapticLoopOptions& Loop,
		const FOpenMobileHapticCapabilities& Capabilities,
		const FOpenMobileHapticsAppleAHAPLimits& Limits
	);

	static FOpenMobileHapticDynamicParameterUpdate ComposeDynamicUpdate(
		const FOpenMobileHapticDynamicParameterUpdate& Update,
		float StaticIntensityScale
	);
};

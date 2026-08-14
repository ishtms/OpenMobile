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
	/** Copies the safety caps from project settings into a plain value the policy can use without holding a UObject. */
	static FOpenMobileHapticsAppleAHAPLimits MakeLimits(
		const UOpenMobileHapticsSettings& Settings
	);

	/** Accepts, repeats, or rejects cooked AHAP using the capabilities seen right now, not the device used during import. */
	static FOpenMobileHapticsAppleAHAPResolution Resolve(
		const UOpenMobileHapticIOSPatternAsset& Asset,
		const FOpenMobileHapticLoopOptions& Loop,
		const FOpenMobileHapticCapabilities& Capabilities,
		const FOpenMobileHapticsAppleAHAPLimits& Limits
	);

	/** Folds the request's static intensity into a live update so Core Haptics receives one consistent value. */
	static FOpenMobileHapticDynamicParameterUpdate ComposeDynamicUpdate(
		const FOpenMobileHapticDynamicParameterUpdate& Update,
		float StaticIntensityScale
	);
};

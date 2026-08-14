#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsSemanticPolicy.h"

enum class EOpenMobileHapticAndroidPredefinedEffect : uint8
{
	Tick,
	Click,
	HeavyClick,
	DoubleClick
};

enum class EOpenMobileHapticsAndroidFallbackOutcome : uint8
{
	Primitive,
	Predefined,
	Semantic,
	BasicVibration,
	NoEffect,
	Rejected
};

struct FOpenMobileHapticsAndroidFallbackResolution
{
	EOpenMobileHapticsAndroidFallbackOutcome Outcome =
		EOpenMobileHapticsAndroidFallbackOutcome::Rejected;
	EOpenMobileHapticAndroidPrimitive Primitive =
		EOpenMobileHapticAndroidPrimitive::Click;
	EOpenMobileHapticAndroidPredefinedEffect PredefinedEffect =
		EOpenMobileHapticAndroidPredefinedEffect::Click;
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::Click;
	TArray<FName> Attempts;
};

class FOpenMobileHapticsAndroidFallbackPolicy final
{
public:
	/** Chooses Android's nearest predefined effect for a semantic request when richer playback isn't available. */
	static EOpenMobileHapticAndroidPredefinedEffect PredefinedForSemantic(
		EOpenMobileHapticsSemanticBehavior Behavior
	);
	/** Checks the OS and vibrator features together, a named Android constant alone doesn't mean this device can play it. */
	static bool SupportsPredefined(
		EOpenMobileHapticAndroidPredefinedEffect Effect,
		const FOpenMobileHapticCapabilities& Capabilities
	);
	/** Walks the allowed Android fallbacks in order and records each attempt for diagnostics also. */
	static FOpenMobileHapticsAndroidFallbackResolution Resolve(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy RequestPolicy,
		bool bAllowPrimitive = true,
		bool bAllowPredefined = true
	);
	/** Starts at the primitive route for callers that already know the exact override path isn't usable. */
	static FOpenMobileHapticsAndroidFallbackResolution ResolvePrimitive(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy RequestPolicy
	);
};

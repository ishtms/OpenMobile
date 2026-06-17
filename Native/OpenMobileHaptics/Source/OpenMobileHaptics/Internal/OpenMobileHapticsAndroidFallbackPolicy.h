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
	static EOpenMobileHapticAndroidPredefinedEffect PredefinedForSemantic(
		EOpenMobileHapticsSemanticBehavior Behavior
	);
	static bool SupportsPredefined(
		EOpenMobileHapticAndroidPredefinedEffect Effect,
		const FOpenMobileHapticCapabilities& Capabilities
	);
	static FOpenMobileHapticsAndroidFallbackResolution Resolve(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy RequestPolicy,
		bool bAllowPrimitive = true,
		bool bAllowPredefined = true
	);
	static FOpenMobileHapticsAndroidFallbackResolution ResolvePrimitive(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy RequestPolicy
	);
};

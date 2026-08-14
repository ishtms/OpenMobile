#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsSemanticBehavior : uint8
{
	Selection,
	ImpactLight,
	ImpactMedium,
	ImpactHeavy,
	ImpactSoft,
	ImpactRigid,
	NotificationSuccess,
	NotificationWarning,
	NotificationError
};

enum class EOpenMobileHapticsSemanticPath : uint8
{
	Unsupported,
	SystemSemantic,
	PredefinedEffect,
	BasicVibration
};

struct FOpenMobileHapticsSemanticDescriptor
{
	EOpenMobileHapticsSemanticBehavior Behavior =
		EOpenMobileHapticsSemanticBehavior::Selection;
	FName Name = TEXT("Selection");
	FName Category = TEXT("UI");
};

struct FOpenMobileHapticsSemanticResolution
{
	EOpenMobileHapticsSemanticPath Path =
		EOpenMobileHapticsSemanticPath::Unsupported;
	bool bFallback = false;
	bool bSuppressWhenUnavailable = false;
};

class FOpenMobileHapticsSemanticPolicy final
{
public:
	/** Converts the public semantic enum into stable behavior, name, and category used by every platform policy. */
	static FOpenMobileHapticsSemanticDescriptor Describe(
		EOpenMobileHapticSemanticEffect Effect
	);
	/** Selects system semantic, predefined, basic, or suppression based on capability and fallback policy. */
	static FOpenMobileHapticsSemanticResolution Resolve(
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
	/** Keeps diagnostic path names stable even if the internal enum order changes later. */
	static FName PathName(EOpenMobileHapticsSemanticPath Path);
};

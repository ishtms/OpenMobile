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
	static FOpenMobileHapticsSemanticDescriptor Describe(
		EOpenMobileHapticSemanticEffect Effect
	);
	static FOpenMobileHapticsSemanticResolution Resolve(
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	);
	static FName PathName(EOpenMobileHapticsSemanticPath Path);
};

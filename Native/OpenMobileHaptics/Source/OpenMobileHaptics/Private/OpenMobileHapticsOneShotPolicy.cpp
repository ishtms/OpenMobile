#include "OpenMobileHapticsOneShotPolicy.h"

FOpenMobileHapticsOneShotResolution
FOpenMobileHapticsOneShotPolicy::Resolve(
	const FOpenMobileHapticCapabilities& Capabilities,
	double DurationSeconds,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	FOpenMobileHapticsOneShotResolution Resolution;
	Resolution.bSuppressWhenUnavailable =
		FallbackPolicy == EOpenMobileHapticFallbackPolicy::NoEffectAllowed;
	const bool bShortPulse = DurationSeconds <= 0.05;
	if (FallbackPolicy != EOpenMobileHapticFallbackPolicy::ExactOnly
		&& bShortPulse
		&& Capabilities.SemanticEffects
			== EOpenMobileHapticSupportState::Supported)
	{
		Resolution.Path = EOpenMobileHapticsOneShotPath::SystemSemantic;
		return Resolution;
	}
	if (FallbackPolicy != EOpenMobileHapticFallbackPolicy::ExactOnly
		&& bShortPulse
		&& Capabilities.PredefinedEffects
			== EOpenMobileHapticSupportState::Supported)
	{
		Resolution.Path = EOpenMobileHapticsOneShotPath::PredefinedEffect;
		return Resolution;
	}
	if (FallbackPolicy != EOpenMobileHapticFallbackPolicy::NoBasicVibration
		&& Capabilities.BasicVibration
			== EOpenMobileHapticSupportState::Supported)
	{
		Resolution.Path = EOpenMobileHapticsOneShotPath::BasicVibration;
	}
	return Resolution;
}

FName FOpenMobileHapticsOneShotPolicy::PathName(
	EOpenMobileHapticsOneShotPath Path
)
{
	switch (Path)
	{
	case EOpenMobileHapticsOneShotPath::SystemSemantic:
		return TEXT("SystemSemantic");
	case EOpenMobileHapticsOneShotPath::PredefinedEffect:
		return TEXT("PredefinedEffect");
	case EOpenMobileHapticsOneShotPath::BasicVibration:
		return TEXT("BasicVibration");
	case EOpenMobileHapticsOneShotPath::Unsupported:
	default:
		return TEXT("Unsupported");
	}
}

#include "OpenMobileHapticsOneShotPolicy.h"

namespace OpenMobileHapticsOneShotPolicyPrivate
{
	bool SupportsClick(const FOpenMobileHapticCapabilities& Capabilities)
	{
		for (const FOpenMobileHapticNamedSupport& Entry :
			Capabilities.PresetSupport)
		{
			if (Entry.Name == TEXT("Click"))
			{
				return Entry.Support
					== EOpenMobileHapticSupportState::Supported;
			}
		}
		return Capabilities.PresetSupport.IsEmpty()
			&& Capabilities.PredefinedEffects
				== EOpenMobileHapticSupportState::Supported;
	}
}

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
		&& OpenMobileHapticsOneShotPolicyPrivate::SupportsClick(Capabilities))
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

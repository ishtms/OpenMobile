#include "OpenMobileHapticsSemanticPolicy.h"

FOpenMobileHapticsSemanticDescriptor
FOpenMobileHapticsSemanticPolicy::Describe(
	EOpenMobileHapticSemanticEffect Effect
)
{
	switch (Effect)
	{
	case EOpenMobileHapticSemanticEffect::ImpactLight:
		return {EOpenMobileHapticsSemanticBehavior::ImpactLight,
			TEXT("ImpactLight"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::ImpactMedium:
		return {EOpenMobileHapticsSemanticBehavior::ImpactMedium,
			TEXT("ImpactMedium"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::ImpactHeavy:
		return {EOpenMobileHapticsSemanticBehavior::ImpactHeavy,
			TEXT("ImpactHeavy"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::ImpactSoft:
		return {EOpenMobileHapticsSemanticBehavior::ImpactSoft,
			TEXT("ImpactSoft"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::ImpactRigid:
		return {EOpenMobileHapticsSemanticBehavior::ImpactRigid,
			TEXT("ImpactRigid"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::NotificationSuccess:
		return {EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("NotificationSuccess"), TEXT("Alerts")};
	case EOpenMobileHapticSemanticEffect::NotificationWarning:
		return {EOpenMobileHapticsSemanticBehavior::NotificationWarning,
			TEXT("NotificationWarning"), TEXT("Alerts")};
	case EOpenMobileHapticSemanticEffect::NotificationError:
		return {EOpenMobileHapticsSemanticBehavior::NotificationError,
			TEXT("NotificationError"), TEXT("Alerts")};
	case EOpenMobileHapticSemanticEffect::Confirm:
		return {EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("Confirm"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::Reject:
		return {EOpenMobileHapticsSemanticBehavior::NotificationError,
			TEXT("Reject"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::Tick:
		return {EOpenMobileHapticsSemanticBehavior::Selection,
			TEXT("Tick"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::Click:
		return {EOpenMobileHapticsSemanticBehavior::ImpactLight,
			TEXT("Click"), TEXT("UI")};
	case EOpenMobileHapticSemanticEffect::Bump:
		return {EOpenMobileHapticsSemanticBehavior::ImpactMedium,
			TEXT("Bump"), TEXT("Gameplay")};
	case EOpenMobileHapticSemanticEffect::Damage:
		return {EOpenMobileHapticsSemanticBehavior::ImpactHeavy,
			TEXT("Damage"), TEXT("Gameplay")};
	case EOpenMobileHapticSemanticEffect::Pickup:
		return {EOpenMobileHapticsSemanticBehavior::ImpactSoft,
			TEXT("Pickup"), TEXT("Gameplay")};
	case EOpenMobileHapticSemanticEffect::Achievement:
		return {EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("Achievement"), TEXT("Alerts")};
	case EOpenMobileHapticSemanticEffect::Selection:
	default:
		return {EOpenMobileHapticsSemanticBehavior::Selection,
			TEXT("Selection"), TEXT("UI")};
	}
}

FOpenMobileHapticsSemanticResolution
FOpenMobileHapticsSemanticPolicy::Resolve(
	const FOpenMobileHapticCapabilities& Capabilities,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	if (Capabilities.SemanticEffects
		== EOpenMobileHapticSupportState::Supported)
	{
		return {EOpenMobileHapticsSemanticPath::SystemSemantic, false, false};
	}

	FOpenMobileHapticsSemanticResolution Resolution;
	Resolution.bSuppressWhenUnavailable =
		FallbackPolicy == EOpenMobileHapticFallbackPolicy::NoEffectAllowed;
	if (FallbackPolicy == EOpenMobileHapticFallbackPolicy::ExactOnly)
	{
		return Resolution;
	}
	if (Capabilities.PredefinedEffects
		== EOpenMobileHapticSupportState::Supported)
	{
		Resolution.Path = EOpenMobileHapticsSemanticPath::PredefinedEffect;
		Resolution.bFallback = true;
		return Resolution;
	}
	if (FallbackPolicy != EOpenMobileHapticFallbackPolicy::NoBasicVibration
		&& Capabilities.BasicVibration
			== EOpenMobileHapticSupportState::Supported)
	{
		Resolution.Path = EOpenMobileHapticsSemanticPath::BasicVibration;
		Resolution.bFallback = true;
	}
	return Resolution;
}

FName FOpenMobileHapticsSemanticPolicy::PathName(
	EOpenMobileHapticsSemanticPath Path
)
{
	switch (Path)
	{
	case EOpenMobileHapticsSemanticPath::SystemSemantic:
		return TEXT("SystemSemantic");
	case EOpenMobileHapticsSemanticPath::PredefinedEffect:
		return TEXT("PredefinedEffect");
	case EOpenMobileHapticsSemanticPath::BasicVibration:
		return TEXT("BasicVibration");
	case EOpenMobileHapticsSemanticPath::Unsupported:
	default:
		return TEXT("Unsupported");
	}
}

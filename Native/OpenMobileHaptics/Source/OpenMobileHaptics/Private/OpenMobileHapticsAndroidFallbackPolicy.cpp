#include "OpenMobileHapticsAndroidFallbackPolicy.h"

namespace OpenMobileHapticsAndroidFallbackPolicyPrivate
{
	bool ResolvePrimitive(
		FName Name,
		EOpenMobileHapticAndroidPrimitive& Primitive
	)
	{
		if (Name == TEXT("Tick"))
		{
			Primitive = EOpenMobileHapticAndroidPrimitive::Tick;
		}
		else if (Name == TEXT("LowTick"))
		{
			Primitive = EOpenMobileHapticAndroidPrimitive::LowTick;
		}
		else if (Name == TEXT("Click"))
		{
			Primitive = EOpenMobileHapticAndroidPrimitive::Click;
		}
		else if (Name == TEXT("Thud"))
		{
			Primitive = EOpenMobileHapticAndroidPrimitive::Thud;
		}
		else if (Name == TEXT("Spin"))
		{
			Primitive = EOpenMobileHapticAndroidPrimitive::Spin;
		}
		else if (Name == TEXT("QuickRise"))
		{
			Primitive = EOpenMobileHapticAndroidPrimitive::QuickRise;
		}
		else if (Name == TEXT("SlowRise"))
		{
			Primitive = EOpenMobileHapticAndroidPrimitive::SlowRise;
		}
		else if (Name == TEXT("QuickFall"))
		{
			Primitive = EOpenMobileHapticAndroidPrimitive::QuickFall;
		}
		else
		{
			return false;
		}
		return true;
	}

	bool ResolvePredefined(
		FName Name,
		EOpenMobileHapticAndroidPredefinedEffect& Effect
	)
	{
		if (Name == TEXT("Tick"))
		{
			Effect = EOpenMobileHapticAndroidPredefinedEffect::Tick;
		}
		else if (Name == TEXT("Click"))
		{
			Effect = EOpenMobileHapticAndroidPredefinedEffect::Click;
		}
		else if (Name == TEXT("HeavyClick"))
		{
			Effect = EOpenMobileHapticAndroidPredefinedEffect::HeavyClick;
		}
		else if (Name == TEXT("DoubleClick"))
		{
			Effect = EOpenMobileHapticAndroidPredefinedEffect::DoubleClick;
		}
		else
		{
			return false;
		}
		return true;
	}

	FName PredefinedName(EOpenMobileHapticAndroidPredefinedEffect Effect)
	{
		switch (Effect)
		{
		case EOpenMobileHapticAndroidPredefinedEffect::Tick:
			return TEXT("Tick");
		case EOpenMobileHapticAndroidPredefinedEffect::HeavyClick:
			return TEXT("HeavyClick");
		case EOpenMobileHapticAndroidPredefinedEffect::DoubleClick:
			return TEXT("DoubleClick");
		case EOpenMobileHapticAndroidPredefinedEffect::Click:
		default:
			return TEXT("Click");
		}
	}

	bool HasSupport(
		FName Name,
		EOpenMobileHapticSupportState OverallSupport,
		const TArray<FOpenMobileHapticNamedSupport>& DetailedSupport
	)
	{
		if (OverallSupport != EOpenMobileHapticSupportState::Supported)
		{
			return false;
		}
		for (const FOpenMobileHapticNamedSupport& Entry :
			DetailedSupport)
		{
			if (Entry.Name == Name)
			{
				return Entry.Support
					== EOpenMobileHapticSupportState::Supported;
			}
		}
		return false;
	}

	bool Allows(
		const UOpenMobileHapticPatternAsset& Pattern,
		EOpenMobileHapticFallbackFloor Floor
	)
	{
		return static_cast<uint8>(Pattern.LowestAllowedFallback)
			>= static_cast<uint8>(Floor);
	}
}

EOpenMobileHapticAndroidPredefinedEffect
FOpenMobileHapticsAndroidFallbackPolicy::PredefinedForSemantic(
	EOpenMobileHapticsSemanticBehavior Behavior
)
{
	switch (Behavior)
	{
	case EOpenMobileHapticsSemanticBehavior::Selection:
	case EOpenMobileHapticsSemanticBehavior::ImpactSoft:
		return EOpenMobileHapticAndroidPredefinedEffect::Tick;
	case EOpenMobileHapticsSemanticBehavior::ImpactLight:
	case EOpenMobileHapticsSemanticBehavior::ImpactRigid:
		return EOpenMobileHapticAndroidPredefinedEffect::Click;
	case EOpenMobileHapticsSemanticBehavior::NotificationSuccess:
		return EOpenMobileHapticAndroidPredefinedEffect::DoubleClick;
	case EOpenMobileHapticsSemanticBehavior::ImpactMedium:
	case EOpenMobileHapticsSemanticBehavior::ImpactHeavy:
	case EOpenMobileHapticsSemanticBehavior::NotificationWarning:
	case EOpenMobileHapticsSemanticBehavior::NotificationError:
	default:
		return EOpenMobileHapticAndroidPredefinedEffect::HeavyClick;
	}
}

bool FOpenMobileHapticsAndroidFallbackPolicy::SupportsPredefined(
	EOpenMobileHapticAndroidPredefinedEffect Effect,
	const FOpenMobileHapticCapabilities& Capabilities
)
{
	return OpenMobileHapticsAndroidFallbackPolicyPrivate::HasSupport(
		OpenMobileHapticsAndroidFallbackPolicyPrivate::PredefinedName(Effect),
		Capabilities.PredefinedEffects,
		Capabilities.PresetSupport
	);
}

FOpenMobileHapticsAndroidFallbackResolution
FOpenMobileHapticsAndroidFallbackPolicy::Resolve(
	const UOpenMobileHapticPatternAsset& Pattern,
	const FOpenMobileHapticCapabilities& Capabilities,
	EOpenMobileHapticFallbackPolicy RequestPolicy,
	bool bAllowPrimitive,
	bool bAllowPredefined
)
{
	using namespace OpenMobileHapticsAndroidFallbackPolicyPrivate;
	FOpenMobileHapticsAndroidFallbackResolution Resolution;
	Resolution.Attempts.Add(TEXT("ExactOverride:Unavailable"));
	if (RequestPolicy == EOpenMobileHapticFallbackPolicy::ExactOnly
		|| Pattern.FallbackPolicy == EOpenMobileHapticFallbackPolicy::ExactOnly)
	{
		Resolution.Attempts.Add(TEXT("Primitive:ExactOnly"));
		return Resolution;
	}
	if (Allows(
		Pattern,
		EOpenMobileHapticFallbackFloor::PrimitiveOrPredefined
	))
	{
		EOpenMobileHapticAndroidPrimitive Primitive;
		if (bAllowPrimitive
			&& OpenMobileHapticsAndroidFallbackPolicyPrivate::ResolvePrimitive(
			Pattern.PrimitiveOrPresetFallback,
			Primitive
		)
			&& HasSupport(
				Pattern.PrimitiveOrPresetFallback,
				Capabilities.Primitives,
				Capabilities.PrimitiveSupport
			))
		{
			Resolution.Outcome =
				EOpenMobileHapticsAndroidFallbackOutcome::Primitive;
			Resolution.Primitive = Primitive;
			Resolution.Attempts.Add(TEXT("Primitive:Selected"));
			return Resolution;
		}
		Resolution.Attempts.Add(
			bAllowPrimitive
				? FName(TEXT("Primitive:Unavailable"))
				: FName(TEXT("Primitive:Skipped"))
		);

		EOpenMobileHapticAndroidPredefinedEffect PredefinedEffect;
		if (bAllowPredefined
			&& ResolvePredefined(
				Pattern.PrimitiveOrPresetFallback,
				PredefinedEffect
			)
			&& HasSupport(
				Pattern.PrimitiveOrPresetFallback,
				Capabilities.PredefinedEffects,
				Capabilities.PresetSupport
			))
		{
			Resolution.Outcome =
				EOpenMobileHapticsAndroidFallbackOutcome::Predefined;
			Resolution.PredefinedEffect = PredefinedEffect;
			Resolution.Attempts.Add(TEXT("Predefined:Selected"));
			return Resolution;
		}
		Resolution.Attempts.Add(
			bAllowPredefined
				? FName(TEXT("Predefined:Unavailable"))
				: FName(TEXT("Predefined:Skipped"))
		);
	}
	else
	{
		Resolution.Attempts.Add(TEXT("Primitive:AssetPolicy"));
		Resolution.Attempts.Add(TEXT("Predefined:AssetPolicy"));
	}

	if (Allows(Pattern, EOpenMobileHapticFallbackFloor::Semantic))
	{
		if (Pattern.bAllowSemanticFallback
			&& Capabilities.SemanticEffects
				== EOpenMobileHapticSupportState::Supported)
		{
			Resolution.Outcome =
				EOpenMobileHapticsAndroidFallbackOutcome::Semantic;
			Resolution.SemanticEffect = Pattern.SemanticFallback;
			Resolution.Attempts.Add(TEXT("Semantic:Selected"));
			return Resolution;
		}
		Resolution.Attempts.Add(TEXT("Semantic:Unavailable"));
	}
	else
	{
		Resolution.Attempts.Add(TEXT("Semantic:AssetPolicy"));
	}

	const bool bNoBasic =
		RequestPolicy == EOpenMobileHapticFallbackPolicy::NoBasicVibration
		|| Pattern.FallbackPolicy
			== EOpenMobileHapticFallbackPolicy::NoBasicVibration;
	if (Allows(Pattern, EOpenMobileHapticFallbackFloor::BasicVibration)
		&& !bNoBasic)
	{
		if (Capabilities.BasicVibration
			== EOpenMobileHapticSupportState::Supported)
		{
			Resolution.Outcome =
				EOpenMobileHapticsAndroidFallbackOutcome::BasicVibration;
			Resolution.Attempts.Add(TEXT("BasicVibration:Selected"));
			return Resolution;
		}
		Resolution.Attempts.Add(TEXT("BasicVibration:Unavailable"));
	}
	else
	{
		Resolution.Attempts.Add(TEXT("BasicVibration:Policy"));
	}

	if (RequestPolicy == EOpenMobileHapticFallbackPolicy::NoEffectAllowed
		|| Pattern.FallbackPolicy
			== EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
	{
		Resolution.Outcome = EOpenMobileHapticsAndroidFallbackOutcome::NoEffect;
		Resolution.Attempts.Add(TEXT("NoEffect:Selected"));
	}
	else
	{
		Resolution.Attempts.Add(TEXT("NoEffect:Policy"));
	}
	return Resolution;
}

FOpenMobileHapticsAndroidFallbackResolution
FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
	const UOpenMobileHapticPatternAsset& Pattern,
	const FOpenMobileHapticCapabilities& Capabilities,
	EOpenMobileHapticFallbackPolicy RequestPolicy
)
{
	return Resolve(Pattern, Capabilities, RequestPolicy);
}

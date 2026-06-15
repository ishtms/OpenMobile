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

	bool HasSupport(
		FName Name,
		const FOpenMobileHapticCapabilities& Capabilities
	)
	{
		if (Capabilities.Primitives
			!= EOpenMobileHapticSupportState::Supported)
		{
			return false;
		}
		for (const FOpenMobileHapticNamedSupport& Entry :
			Capabilities.PrimitiveSupport)
		{
			if (Entry.Name == Name)
			{
				return Entry.Support
					== EOpenMobileHapticSupportState::Supported;
			}
		}
		return false;
	}
}

FOpenMobileHapticsAndroidFallbackResolution
FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
	const UOpenMobileHapticPatternAsset& Pattern,
	const FOpenMobileHapticCapabilities& Capabilities,
	EOpenMobileHapticFallbackPolicy RequestPolicy
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
	if (static_cast<uint8>(Pattern.LowestAllowedFallback)
		>= static_cast<uint8>(
			EOpenMobileHapticFallbackFloor::PrimitiveOrPredefined
		))
	{
		EOpenMobileHapticAndroidPrimitive Primitive;
		if (OpenMobileHapticsAndroidFallbackPolicyPrivate::ResolvePrimitive(
			Pattern.PrimitiveOrPresetFallback,
			Primitive
		)
			&& HasSupport(Pattern.PrimitiveOrPresetFallback, Capabilities))
		{
			Resolution.Outcome =
				EOpenMobileHapticsAndroidFallbackOutcome::Primitive;
			Resolution.Primitive = Primitive;
			Resolution.Attempts.Add(TEXT("Primitive:Selected"));
			return Resolution;
		}
		Resolution.Attempts.Add(TEXT("Primitive:Unavailable"));
	}
	else
	{
		Resolution.Attempts.Add(TEXT("Primitive:AssetPolicy"));
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

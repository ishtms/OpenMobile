#include "OpenMobileHapticsFallbackPolicy.h"

#include "OpenMobileHapticPatternAsset.h"

namespace OpenMobileHapticsFallbackPolicyPrivate
{
	FName PathName(EOpenMobileHapticsFallbackPath Path)
	{
		switch (Path)
		{
		case EOpenMobileHapticsFallbackPath::ExactOverride:
			return TEXT("ExactOverride");
		case EOpenMobileHapticsFallbackPath::PortableRich:
			return TEXT("PortableRich");
		case EOpenMobileHapticsFallbackPath::PrimitiveOrPredefined:
			return TEXT("PrimitiveOrPredefined");
		case EOpenMobileHapticsFallbackPath::Semantic:
			return TEXT("Semantic");
		case EOpenMobileHapticsFallbackPath::BasicVibration:
			return TEXT("BasicVibration");
		case EOpenMobileHapticsFallbackPath::NoEffect:
			return TEXT("NoEffect");
		default:
			return TEXT("Rejected");
		}
	}

	void AddAttempt(
		FOpenMobileHapticsFallbackResolution& Resolution,
		EOpenMobileHapticsFallbackPath Path,
		FName Reason,
		bool bSelected = false
	)
	{
		Resolution.Attempts.Add({Path, Reason, bSelected});
		if (bSelected)
		{
			Resolution.Path = Path;
			Resolution.ResolvedPath = PathName(Path);
			Resolution.bSuccessfulOutcome = true;
		}
	}

	bool IsDetailedSupportAvailable(
		FName Name,
		const TArray<FOpenMobileHapticNamedSupport>& Support
	)
	{
		for (const FOpenMobileHapticNamedSupport& Entry : Support)
		{
			if (Entry.Name == Name)
			{
				return Entry.Support
					== EOpenMobileHapticSupportState::Supported;
			}
		}
		return false;
	}

	bool HasDetailedSupportEntry(
		FName Name,
		const TArray<FOpenMobileHapticNamedSupport>& Support
	)
	{
		return Support.ContainsByPredicate(
			[Name](const FOpenMobileHapticNamedSupport& Entry)
			{
				return Entry.Name == Name;
			}
		);
	}

	bool SupportsNamedFallback(
		FName Name,
		const FOpenMobileHapticCapabilities& Capabilities
	)
	{
		if (Name.IsNone())
		{
			return false;
		}
		const bool bHasDetailedEntry = HasDetailedSupportEntry(
			Name,
			Capabilities.PrimitiveSupport
		) || HasDetailedSupportEntry(Name, Capabilities.PresetSupport);
		if (bHasDetailedEntry)
		{
			return IsDetailedSupportAvailable(
				Name,
				Capabilities.PrimitiveSupport
			) || IsDetailedSupportAvailable(Name, Capabilities.PresetSupport);
		}
		return Capabilities.Primitives
				== EOpenMobileHapticSupportState::Supported
			|| Capabilities.PredefinedEffects
				== EOpenMobileHapticSupportState::Supported;
	}

	bool SupportsPortableRich(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities
	)
	{
		if (Capabilities.WaveformTiming
			== EOpenMobileHapticSupportState::Supported)
		{
			return true;
		}
		if (Capabilities.RichHaptics
			!= EOpenMobileHapticSupportState::Supported)
		{
			return false;
		}
		for (const FOpenMobileHapticCookedPatternEvent& Event :
			Pattern.GetCookedPattern().Events)
		{
			if (Event.Type == EOpenMobileHapticPatternEventType::Transient
				&& Capabilities.TransientEvents
					!= EOpenMobileHapticSupportState::Supported)
			{
				return false;
			}
			if (Event.Type == EOpenMobileHapticPatternEventType::Continuous
				&& Capabilities.ContinuousEvents
					!= EOpenMobileHapticSupportState::Supported)
			{
				return false;
			}
		}
		return true;
	}

	bool AllowsStep(
		const UOpenMobileHapticPatternAsset& Pattern,
		EOpenMobileHapticsFallbackPath Path,
		bool bExactOnly,
		bool bNoBasic
	)
	{
		if (bExactOnly)
		{
			return false;
		}
		if (Path == EOpenMobileHapticsFallbackPath::BasicVibration
			&& bNoBasic)
		{
			return false;
		}
		return static_cast<uint8>(Path) - 1
			<= static_cast<uint8>(Pattern.LowestAllowedFallback);
	}
}

FOpenMobileHapticsFallbackResolution
FOpenMobileHapticsFallbackPolicy::Resolve(
	const UOpenMobileHapticPatternAsset& Pattern,
	const FOpenMobileHapticsPlatformOverrideResolution& Override,
	const FOpenMobileHapticCapabilities& Capabilities,
	EOpenMobileHapticFallbackPolicy RequestPolicy
)
{
	using namespace OpenMobileHapticsFallbackPolicyPrivate;
	FOpenMobileHapticsFallbackResolution Resolution;
	const bool bExactOnly =
		RequestPolicy == EOpenMobileHapticFallbackPolicy::ExactOnly
		|| Pattern.FallbackPolicy
			== EOpenMobileHapticFallbackPolicy::ExactOnly;
	const bool bNoBasic =
		RequestPolicy == EOpenMobileHapticFallbackPolicy::NoBasicVibration
		|| Pattern.FallbackPolicy
			== EOpenMobileHapticFallbackPolicy::NoBasicVibration;
	const bool bAllowNoEffect =
		RequestPolicy == EOpenMobileHapticFallbackPolicy::NoEffectAllowed
		|| Pattern.FallbackPolicy
			== EOpenMobileHapticFallbackPolicy::NoEffectAllowed;
	if (Override.Path == EOpenMobileHapticsPlatformOverridePath::ExactOverride)
	{
		AddAttempt(
			Resolution,
			EOpenMobileHapticsFallbackPath::ExactOverride,
			TEXT("Selected"),
			true
		);
		return Resolution;
	}
	AddAttempt(
		Resolution,
		EOpenMobileHapticsFallbackPath::ExactOverride,
		Override.Reason.IsNone() ? FName(TEXT("Unavailable")) : Override.Reason
	);
	if (bExactOnly)
	{
		Resolution.Path = EOpenMobileHapticsFallbackPath::Rejected;
		return Resolution;
	}

	const bool bPortableAllowed = AllowsStep(
		Pattern,
		EOpenMobileHapticsFallbackPath::PortableRich,
		bExactOnly,
		bNoBasic
	);
	if (bPortableAllowed && SupportsPortableRich(Pattern, Capabilities))
	{
		AddAttempt(
			Resolution,
			EOpenMobileHapticsFallbackPath::PortableRich,
			TEXT("Selected"),
			true
		);
		return Resolution;
	}
	AddAttempt(
		Resolution,
		EOpenMobileHapticsFallbackPath::PortableRich,
		bPortableAllowed ? FName(TEXT("Unsupported")) : FName(TEXT("AssetPolicy"))
	);

	const bool bPrimitiveAllowed = AllowsStep(
		Pattern,
		EOpenMobileHapticsFallbackPath::PrimitiveOrPredefined,
		bExactOnly,
		bNoBasic
	);
	if (bPrimitiveAllowed && SupportsNamedFallback(
		Pattern.PrimitiveOrPresetFallback,
		Capabilities
	))
	{
		AddAttempt(
			Resolution,
			EOpenMobileHapticsFallbackPath::PrimitiveOrPredefined,
			TEXT("Selected"),
			true
		);
		return Resolution;
	}
	AddAttempt(
		Resolution,
		EOpenMobileHapticsFallbackPath::PrimitiveOrPredefined,
		bPrimitiveAllowed
			? Pattern.PrimitiveOrPresetFallback.IsNone()
				? FName(TEXT("NotDeclared"))
				: FName(TEXT("Unsupported"))
			: FName(TEXT("AssetPolicy"))
	);

	const bool bSemanticAllowed = AllowsStep(
		Pattern,
		EOpenMobileHapticsFallbackPath::Semantic,
		bExactOnly,
		bNoBasic
	);
	if (bSemanticAllowed && Pattern.bAllowSemanticFallback
		&& Capabilities.SemanticEffects
			== EOpenMobileHapticSupportState::Supported)
	{
		AddAttempt(
			Resolution,
			EOpenMobileHapticsFallbackPath::Semantic,
			TEXT("Selected"),
			true
		);
		return Resolution;
	}
	AddAttempt(
		Resolution,
		EOpenMobileHapticsFallbackPath::Semantic,
		bSemanticAllowed
			? Pattern.bAllowSemanticFallback
				? FName(TEXT("Unsupported"))
				: FName(TEXT("NotDeclared"))
			: FName(TEXT("AssetPolicy"))
	);

	const bool bBasicAllowed = AllowsStep(
		Pattern,
		EOpenMobileHapticsFallbackPath::BasicVibration,
		bExactOnly,
		bNoBasic
	);
	if (bBasicAllowed && Capabilities.BasicVibration
		== EOpenMobileHapticSupportState::Supported)
	{
		AddAttempt(
			Resolution,
			EOpenMobileHapticsFallbackPath::BasicVibration,
			TEXT("Selected"),
			true
		);
		return Resolution;
	}
	AddAttempt(
		Resolution,
		EOpenMobileHapticsFallbackPath::BasicVibration,
		bBasicAllowed ? FName(TEXT("Unsupported")) : FName(TEXT("Policy"))
	);

	if (bAllowNoEffect)
	{
		AddAttempt(
			Resolution,
			EOpenMobileHapticsFallbackPath::NoEffect,
			TEXT("Selected"),
			true
		);
		return Resolution;
	}
	AddAttempt(
		Resolution,
		EOpenMobileHapticsFallbackPath::NoEffect,
		TEXT("RequestPolicy")
	);
	Resolution.Path = EOpenMobileHapticsFallbackPath::Rejected;
	return Resolution;
}

TArray<FName> FOpenMobileHapticsFallbackPolicy::MakeDiagnosticTrace(
	const FOpenMobileHapticsFallbackResolution& Resolution
)
{
	using namespace OpenMobileHapticsFallbackPolicyPrivate;
	TArray<FName> Trace;
	Trace.Reserve(Resolution.Attempts.Num());
	for (const FOpenMobileHapticsFallbackAttempt& Attempt :
		Resolution.Attempts)
	{
		Trace.Add(*FString::Printf(
			TEXT("%s:%s%s"),
			*PathName(Attempt.Path).ToString(),
			*Attempt.Reason.ToString(),
			Attempt.bSelected ? TEXT(":Selected") : TEXT("")
		));
	}
	return Trace;
}

#include "OpenMobileHapticsPlatformOverridePolicy.h"

#include "OpenMobileHapticPatternAsset.h"

namespace OpenMobileHapticsPlatformOverridePolicyPrivate
{
	FOpenMobileHapticsPlatformOverrideResolution Fallback(
		const UOpenMobileHapticPatternAsset& Pattern,
		EOpenMobileHapticFallbackPolicy Policy,
		FName Reason
	)
	{
		FOpenMobileHapticsPlatformOverrideResolution Resolution;
		Resolution.Reason = Reason;
		if (Policy == EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
		{
			Resolution.Path =
				EOpenMobileHapticsPlatformOverridePath::NoEffect;
		}
		else if (Policy != EOpenMobileHapticFallbackPolicy::ExactOnly
			&& Pattern.IsDerivedDataCurrent())
		{
			Resolution.Path =
				EOpenMobileHapticsPlatformOverridePath::PortablePattern;
		}
		return Resolution;
	}
}

FOpenMobileHapticsPlatformOverrideResolution
FOpenMobileHapticsPlatformOverridePolicy::Resolve(
	const UOpenMobileHapticPatternAsset& Pattern,
	EOpenMobileHapticOverridePlatform Platform,
	int32 OSVersion,
	const FOpenMobileHapticCapabilities& Capabilities,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	using namespace OpenMobileHapticsPlatformOverridePolicyPrivate;
	const FSoftObjectPath OverridePath = Pattern.GetOverrideForPlatform(Platform);
	if (OverridePath.IsNull())
	{
		return Fallback(Pattern, FallbackPolicy, TEXT("MissingOverride"));
	}
	const UOpenMobileHapticPlatformPatternAsset* Override =
		Cast<UOpenMobileHapticPlatformPatternAsset>(
			OverridePath.ResolveObject()
		);
	if (!Override || Override->GetOverridePlatform() != Platform)
	{
		return Fallback(Pattern, FallbackPolicy, TEXT("InvalidOverride"));
	}
	TArray<FString> Errors;
	if (!Override->Validate(Errors))
	{
		return Fallback(Pattern, FallbackPolicy, TEXT("InvalidOverride"));
	}
	if (OSVersion < Override->GetMinimumOSVersion())
	{
		return Fallback(Pattern, FallbackPolicy, TEXT("OSVersion"));
	}
	if (!Override->Supports(Capabilities, OSVersion))
	{
		return Fallback(Pattern, FallbackPolicy, TEXT("Capability"));
	}

	FOpenMobileHapticsPlatformOverrideResolution Resolution;
	Resolution.Path = EOpenMobileHapticsPlatformOverridePath::ExactOverride;
	Resolution.OverrideAsset = OverridePath;
	Resolution.Reason = TEXT("ExactOverride");
	return Resolution;
}

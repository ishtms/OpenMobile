#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsPlatformOverridePolicy.h"

class UOpenMobileHapticPatternAsset;

enum class EOpenMobileHapticsFallbackPath : uint8
{
	ExactOverride,
	PortableRich,
	PrimitiveOrPredefined,
	Semantic,
	BasicVibration,
	NoEffect,
	Rejected
};

struct FOpenMobileHapticsFallbackAttempt
{
	EOpenMobileHapticsFallbackPath Path =
		EOpenMobileHapticsFallbackPath::Rejected;
	FName Reason;
	bool bSelected = false;
};

struct FOpenMobileHapticsFallbackResolution
{
	EOpenMobileHapticsFallbackPath Path =
		EOpenMobileHapticsFallbackPath::Rejected;
	FName ResolvedPath;
	TArray<FOpenMobileHapticsFallbackAttempt> Attempts;
	bool bSuccessfulOutcome = false;
};

class FOpenMobileHapticsFallbackPolicy final
{
public:
	static FOpenMobileHapticsFallbackResolution Resolve(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticsPlatformOverrideResolution& Override,
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy RequestPolicy
	);
	static TArray<FName> MakeDiagnosticTrace(
		const FOpenMobileHapticsFallbackResolution& Resolution
	);
};

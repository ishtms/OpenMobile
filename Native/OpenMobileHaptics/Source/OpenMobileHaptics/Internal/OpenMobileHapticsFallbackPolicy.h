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
	/** Selects the first allowed playback route and preserves rejected attempts, which makes device-specific fallback explainable. */
	static FOpenMobileHapticsFallbackResolution Resolve(
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticsPlatformOverrideResolution& Override,
		const FOpenMobileHapticCapabilities& Capabilities,
		EOpenMobileHapticFallbackPolicy RequestPolicy
	);
	/** Reduces the attempt records to stable names that can cross logs and Blueprint diagnostics without exposing internal enums. */
	static TArray<FName> MakeDiagnosticTrace(
		const FOpenMobileHapticsFallbackResolution& Resolution
	);
};

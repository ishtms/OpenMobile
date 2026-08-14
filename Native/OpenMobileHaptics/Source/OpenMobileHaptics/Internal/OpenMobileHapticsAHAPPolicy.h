#pragma once

#include "CoreMinimal.h"

enum class EOpenMobileHapticsAHAPError : uint8
{
	None,
	SourceTooLarge,
	MalformedJson,
	MissingVersion,
	UnsupportedVersion,
	MissingPattern,
	MissingKey,
	UnsupportedKey,
	InvalidStructure,
	Nonfinite,
	InvalidValue,
	LimitExceeded,
	ExternalResourcePath
};

struct FOpenMobileHapticsAHAPLimits
{
	int32 MaximumSourceBytes = 256 * 1024;
	int32 MaximumPatternEntries = 4096;
	int32 MaximumParameters = 4096;
	int32 MaximumParameterCurves = 128;
	int32 MaximumCurvePoints = 4096;
	double MaximumDurationSeconds = 300.0;
};

struct FOpenMobileHapticsAHAPResource
{
	FString NormalizedJson;
	double DurationSeconds = 0.0;
	int32 PatternEntryCount = 0;
	int32 HapticEventCount = 0;
	int32 AudioEventCount = 0;
	int32 ParameterCount = 0;
	int32 ParameterCurveCount = 0;
	TArray<FString> ExternalAudioResourcePaths;
	bool bContainsHapticEvents = false;
	bool bContainsAudioEvents = false;
	bool bContainsCustomAudioEvents = false;
	bool bRequiresAdvancedPlayer = false;
};

struct FOpenMobileHapticsAHAPNormalizationResult
{
	bool bSuccess = false;
	EOpenMobileHapticsAHAPError Error =
		EOpenMobileHapticsAHAPError::None;
	FString FieldPath;
	FOpenMobileHapticsAHAPResource Resource;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsAHAPPolicy final
{
public:
	/** Parses and normalizes AHAP before it reaches Apple code, so malformed or oversized input can't slip into the native engine. */
	static FOpenMobileHapticsAHAPNormalizationResult Normalize(
		const FString& Source,
		const FOpenMobileHapticsAHAPLimits& Limits = {},
		bool bAllowExternalAudioResources = false
	);

	/** Turns the exact failed field into something an importer or caller can act on without knowing the parser internals. */
	static FString DescribeError(
		const FOpenMobileHapticsAHAPNormalizationResult& Result
	);
};

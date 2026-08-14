#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPlatformAssets.h"

enum class EOpenMobileHapticsAppleAudioResourceError : uint8
{
	None,
	UnsafePath,
	UnsupportedFormat,
	InvalidContainer,
	MissingResource,
	UnexpectedResource,
	DuplicateResource,
	ResourceTooLarge,
	TotalSizeExceeded,
	ResourceCountExceeded
};

struct FOpenMobileHapticsAppleAudioResourceLimits
{
	int32 MaximumResourceCount = 16;
	int64 MaximumResourceBytes = 4 * 1024 * 1024;
	int64 MaximumTotalBytes = 16 * 1024 * 1024;
	int32 MaximumRelativePathCharacters = 256;
};

struct FOpenMobileHapticsAppleAudioResourceValidation
{
	bool bSuccess = false;
	EOpenMobileHapticsAppleAudioResourceError Error =
		EOpenMobileHapticsAppleAudioResourceError::None;
	FString RelativePath;
	int64 TotalBytes = 0;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsAppleAudioResourcePolicy final
{
public:
	/** Cleans an AHAP audio path without allowing absolute paths or traversal outside the cooked resource set. */
	static bool NormalizeRelativePath(
		const FString& Path,
		FString& OutNormalizedPath,
		const FOpenMobileHapticsAppleAudioResourceLimits& Limits = {}
	);

	/** Confirms every referenced audio file is present exactly once and stays inside the configured size caps. */
	static FOpenMobileHapticsAppleAudioResourceValidation Validate(
		const TArray<FString>& ExpectedRelativePaths,
		const TArray<FOpenMobileHapticIOSAudioResource>& Resources,
		const FOpenMobileHapticsAppleAudioResourceLimits& Limits = {}
	);

	/** Gives importers the failed resource or rule, otherwise all validation failures would look identical. */
	static FString DescribeError(
		const FOpenMobileHapticsAppleAudioResourceValidation& Result
	);
};

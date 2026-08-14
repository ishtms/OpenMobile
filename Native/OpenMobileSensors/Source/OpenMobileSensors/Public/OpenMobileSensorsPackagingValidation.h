#pragma once

#include "CoreMinimal.h"

class UOpenMobileSensorsSettings;

enum class EOpenMobileSensorsPackagingTarget : uint8
{
	Android,
	IOS
};

enum class EOpenMobileSensorsPackagingIssueCode : uint8
{
	MissingAndroidActivityDeclaration,
	MissingAndroidHighSamplingDeclaration,
	MismatchedAndroidHostDeclaration,
	DuplicateAndroidHostDeclaration,
	MissingIOSMotionDeclaration,
	MissingIOSMotionHostFallback,
	MissingIOSMotionUsageDescription,
	MismatchedIOSMotionUsageDescription,
	DuplicateIOSMotionUsageDescription,
	UnsafeShippingDevelopmentInput
};

struct OPENMOBILESENSORS_API FOpenMobileSensorsPackagingIssue
{
	EOpenMobileSensorsPackagingIssueCode Code =
		EOpenMobileSensorsPackagingIssueCode::MissingAndroidActivityDeclaration;
	FString Message;
};

struct OPENMOBILESENSORS_API FOpenMobileSensorsPackagingContext
{
	EOpenMobileSensorsPackagingTarget Target =
		EOpenMobileSensorsPackagingTarget::Android;
	bool bShipping = false;
	bool bSensorsPlatformPackagingEnabled = false;
	bool bActivityProviderPackagingEnabled = false;
	bool bIOSHostPlistFallbackRequired = false;
	TArray<FString> AndroidHostDeclarations;
	FString IOSHostAdditionalPlistData;
};

class OPENMOBILESENSORS_API FOpenMobileSensorsPackagingValidator
{
public:
	/** Run this before packaging when sensor permissions or development input are configured. You'll get every actionable issue in Out Issues, not only the first one. */
	static bool Validate(
		const UOpenMobileSensorsSettings& Settings,
		const FOpenMobileSensorsPackagingContext& Context,
		TArray<FOpenMobileSensorsPackagingIssue>& OutIssues
	);
};

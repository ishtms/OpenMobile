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
	static bool Validate(
		const UOpenMobileSensorsSettings& Settings,
		const FOpenMobileSensorsPackagingContext& Context,
		TArray<FOpenMobileSensorsPackagingIssue>& OutIssues
	);
};

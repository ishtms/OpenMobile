#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

enum class EOpenMobileDeviceBuildValidationTarget : uint8
{
	Development,
	Shipping,
	StoreSubmission
};

enum class EOpenMobileDeviceBuildValidationSeverity : uint8
{
	Warning,
	Error
};

struct OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceBuildValidationIssue
{
	FName Code;
	EOpenMobileDeviceBuildValidationSeverity Severity =
		EOpenMobileDeviceBuildValidationSeverity::Warning;
	FString Message;
	bool bBlocksBuild = false;
};

struct OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceBuildValidationReport
{
	EOpenMobileDeviceBuildValidationTarget Target =
		EOpenMobileDeviceBuildValidationTarget::Development;
	TArray<FOpenMobileDeviceBuildValidationIssue> Issues;

	bool HasBlockingIssues() const;
};

struct OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceBuildValidationPlatform
{
	FString Name;
	bool bConfigHierarchyResolved = false;
	bool bUsePlatformDefaultLowStorageThreshold = true;
	int64 LowStorageThresholdBytes = 512ll * 1024 * 1024;
	int64 LowStorageRecoveryHysteresisBytes = 64ll * 1024 * 1024;
	float LowStorageFallbackPollingIntervalSeconds = 30.0f;
	float FallbackPollingIntervalSeconds = 1.0f;
	int32 EndpointReachabilityMaximumConcurrentRequests = 4;
	TArray<FString> ReachabilityEndpoints;
	TArray<FString> DeclaredUrlSchemes;
	TArray<FString> DeclaredAndroidIntentActions;
	TArray<FString> DeclaredAndroidPackages;
};

struct OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceBuildValidationNative
{
	bool bAndroidPluginReceiptDeclared = false;
	TArray<FString> AndroidPermissions;
	TArray<FString> AndroidDependencies;
	bool bIOSPluginReceiptDeclared = false;
	TArray<FString> IOSFrameworks;
	FString IOSDiskSpacePrivacyReason;
};

struct OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceBuildValidationInput
{
	FName SettingsCategory;
	FName SettingsSection;
	FString SettingsDisplayName;
	FName SettingsConfigName;
	bool bAllSettingsPropertiesStaged = false;
	TArray<FOpenMobileDeviceBuildValidationPlatform> Platforms;
	FOpenMobileDeviceBuildValidationNative Native;
};

class OPENMOBILEDEVICEEDITOR_API IOpenMobileDeviceBuildValidationContributor
	: public IModularFeature
{
public:
	static FName GetModularFeatureName();

	virtual FName GetContributorName() const = 0;
	virtual FOpenMobileDeviceBuildValidationInput CaptureProjectInput() const = 0;
	virtual FOpenMobileDeviceBuildValidationReport Validate(
		const FOpenMobileDeviceBuildValidationInput& Input,
		EOpenMobileDeviceBuildValidationTarget Target
	) const = 0;
};

class OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceBuildValidation final
	: public IOpenMobileDeviceBuildValidationContributor
{
public:
	virtual FName GetContributorName() const override;
	virtual FOpenMobileDeviceBuildValidationInput CaptureProjectInput()
		const override;
	virtual FOpenMobileDeviceBuildValidationReport Validate(
		const FOpenMobileDeviceBuildValidationInput& Input,
		EOpenMobileDeviceBuildValidationTarget Target
	) const override;

	static FOpenMobileDeviceBuildValidationReport ValidateFixture(
		const FString& Path,
		EOpenMobileDeviceBuildValidationTarget Target
	);
	static bool LoadPlatformSettingsFromConfigHierarchy(
		const FString& EngineConfigDirectory,
		const FString& SourceConfigDirectory,
		const TCHAR* PlatformName,
		FOpenMobileDeviceBuildValidationPlatform& OutPlatform
	);
};

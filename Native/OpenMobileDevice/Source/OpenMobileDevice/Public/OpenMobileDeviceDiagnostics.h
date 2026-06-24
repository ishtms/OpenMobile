#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"

enum class EOpenMobileDeviceDiagnosticIssueSeverity : uint8
{
	Warning,
	Error
};

struct OPENMOBILEDEVICE_API FOpenMobileDeviceDiagnosticError
{
	FDateTime OccurredAtUtc;
	FName Operation;
	EOpenMobileErrorCode Code = EOpenMobileErrorCode::None;
};

struct OPENMOBILEDEVICE_API FOpenMobileDeviceDiagnosticConfigurationIssue
{
	FName Code;
	EOpenMobileDeviceDiagnosticIssueSeverity Severity =
		EOpenMobileDeviceDiagnosticIssueSeverity::Warning;
};

struct OPENMOBILEDEVICE_API FOpenMobileDeviceDiagnosticControlLease
{
	FName Name;
	int32 Count = 0;
};

struct OPENMOBILEDEVICE_API FOpenMobileDeviceDiagnosticsSnapshot
{
	FDateTime CapturedAtUtc;
	FString OpenMobileVersion;
	FName BackendName;
	FOpenMobileDeviceInformationSnapshot DeviceInformation;
	FOpenMobileApplicationMetadataSnapshot ApplicationMetadata;
	TArray<FOpenMobileDeviceCapability> Capabilities;
	FOpenMobilePowerSnapshot Power;
	FOpenMobileMemorySnapshot Memory;
	FOpenMobileStorageSnapshot Storage;
	FOpenMobileNetworkPathSnapshot Network;
	FOpenMobileWindowDisplaySnapshot Window;
	FOpenMobileAppearanceSnapshot Appearance;
	FOpenMobileAccessibilitySnapshot Accessibility;
	TArray<EOpenMobileDeviceMonitoringGroup> ActiveMonitoringGroups;
	TArray<FOpenMobileDeviceDiagnosticControlLease> ControlLeases;
	TArray<FOpenMobileDeviceDiagnosticError> RecentErrors;
	TArray<FOpenMobileDeviceDiagnosticConfigurationIssue> ConfigurationIssues;
	bool bTruncated = false;
};

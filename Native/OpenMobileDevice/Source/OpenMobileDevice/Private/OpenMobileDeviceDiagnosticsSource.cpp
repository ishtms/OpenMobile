#include "OpenMobileDeviceDiagnosticsSource.h"

#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBrightnessControlService.h"
#include "OpenMobileDeviceKeepScreenAwakeControlService.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceNativeConfigurationPolicy.h"
#include "OpenMobileDeviceOrientationControlService.h"
#include "OpenMobileDeviceRefreshRateControlService.h"
#include "OpenMobileDeviceSettings.h"
#include "OpenMobileDeviceSnapshotService.h"
#include "OpenMobileDeviceSystemUiControlService.h"

namespace OpenMobileDeviceDiagnosticsSourcePrivate
{
	constexpr int32 MaximumCapabilityCount = 128;
	constexpr int32 MaximumRecentErrorCount = 16;
	constexpr int32 MaximumConfigurationIssueCount = 32;
	TArray<FOpenMobileDeviceDiagnosticError> RecentErrors;

	void AddIssue(
		TArray<FOpenMobileDeviceDiagnosticConfigurationIssue>& Issues,
		FName Code,
		EOpenMobileDeviceDiagnosticIssueSeverity Severity =
			EOpenMobileDeviceDiagnosticIssueSeverity::Warning
	)
	{
		if (Issues.Num() >= MaximumConfigurationIssueCount
			|| Issues.ContainsByPredicate([Code](
				const FOpenMobileDeviceDiagnosticConfigurationIssue& Issue
			)
			{
				return Issue.Code == Code;
			}))
		{
			return;
		}
		Issues.Add({Code, Severity});
	}

	TArray<FOpenMobileDeviceDiagnosticConfigurationIssue>
	CaptureConfigurationIssues()
	{
		const UOpenMobileDeviceSettings* Settings =
			GetDefault<UOpenMobileDeviceSettings>();
		TArray<FOpenMobileDeviceDiagnosticConfigurationIssue> Issues;
		if (!FMath::IsFinite(Settings->FallbackPollingIntervalSeconds)
			|| Settings->FallbackPollingIntervalSeconds
				!= Settings->GetValidatedFallbackPollingIntervalSeconds())
		{
			AddIssue(
				Issues,
				TEXT("Device.Configuration.InvalidPollingInterval")
			);
		}
		if (!FMath::IsFinite(Settings->LowStorageFallbackPollingIntervalSeconds)
			|| Settings->LowStorageFallbackPollingIntervalSeconds
				!= Settings->GetValidatedLowStorageFallbackPollingIntervalSeconds())
		{
			AddIssue(
				Issues,
				TEXT("Device.Configuration.InvalidStoragePollingInterval")
			);
		}
		if (Settings->EndpointReachabilityMaximumConcurrentRequests
			!= Settings->GetValidatedEndpointReachabilityMaximumConcurrentRequests())
		{
			AddIssue(
				Issues,
				TEXT("Device.Configuration.InvalidEndpointConcurrency")
			);
		}
		if (!Settings->bUsePlatformDefaultLowStorageThreshold
			&& Settings->LowStorageThresholdBytes < 0)
		{
			AddIssue(
				Issues,
				TEXT("Device.Configuration.InvalidStorageThreshold")
			);
		}
		if (Settings->LowStorageRecoveryHysteresisBytes < 0)
		{
			AddIssue(
				Issues,
				TEXT("Device.Configuration.InvalidStorageHysteresis")
			);
		}
		if (Settings->DeclaredUrlSchemes.Num() > 50)
		{
			AddIssue(Issues, TEXT("Device.Configuration.TooManyUrlSchemes"));
		}
		for (const FString& Scheme : Settings->DeclaredUrlSchemes)
		{
			if (!FOpenMobileDeviceNativeConfigurationPolicy::IsValidUrlScheme(
				Scheme
			))
			{
				AddIssue(Issues, TEXT("Device.Configuration.InvalidUrlScheme"));
				break;
			}
		}
		for (const FString& Action : Settings->DeclaredAndroidIntentActions)
		{
			if (!FOpenMobileDeviceNativeConfigurationPolicy::IsValidAndroidIntentAction(
				Action
			))
			{
				AddIssue(Issues, TEXT("Device.Configuration.InvalidIntentAction"));
				break;
			}
		}
		for (const FString& Package : Settings->DeclaredAndroidPackages)
		{
			if (!FOpenMobileDeviceNativeConfigurationPolicy::IsValidAndroidPackage(
				Package
			))
			{
				AddIssue(Issues, TEXT("Device.Configuration.InvalidPackage"));
				break;
			}
		}
		return Issues;
	}

	void AddLease(
		FOpenMobileDeviceDiagnosticsSnapshot& Snapshot,
		FName Name,
		int32 Count
	)
	{
		if (Count > 0)
		{
			Snapshot.ControlLeases.Add({Name, Count});
		}
	}
}

FOpenMobileDeviceDiagnosticsSnapshot
FOpenMobileDeviceDiagnosticsSource::Capture()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceDiagnosticsSourcePrivate;
	FOpenMobileDeviceDiagnosticsSnapshot Snapshot;
	Snapshot.CapturedAtUtc = FDateTime::UtcNow();
	if (IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend())
	{
		Snapshot.BackendName = Backend->GetBackendName();
		for (FName CapabilityName : FOpenMobileDeviceCapabilityNames::GetAll())
		{
			if (Snapshot.Capabilities.Num() >= MaximumCapabilityCount)
			{
				Snapshot.bTruncated = true;
				break;
			}
			Snapshot.Capabilities.Add(Backend->GetCapability(CapabilityName));
		}
	}
	Snapshot.DeviceInformation =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	Snapshot.ApplicationMetadata =
		FOpenMobileDeviceSnapshotService::GetApplicationMetadataSnapshot();
	Snapshot.Power = FOpenMobileDeviceSnapshotService::GetPowerSnapshot();
	Snapshot.Memory = FOpenMobileDeviceSnapshotService::GetMemorySnapshot();
	Snapshot.Storage = FOpenMobileDeviceSnapshotService::GetStorageSnapshot();
	Snapshot.Network = FOpenMobileDeviceSnapshotService::GetNetworkPathSnapshot();
	Snapshot.Window = FOpenMobileDeviceSnapshotService::GetWindowDisplaySnapshot();
	Snapshot.Appearance = FOpenMobileDeviceSnapshotService::GetAppearanceSnapshot();
	Snapshot.Accessibility =
		FOpenMobileDeviceSnapshotService::GetAccessibilitySnapshot();
	Snapshot.ActiveMonitoringGroups =
		FOpenMobileDeviceMonitoringService::GetActiveGroupsForDiagnostics();
	AddLease(
		Snapshot,
		TEXT("Brightness"),
		FOpenMobileDeviceBrightnessControlService::GetActiveRequestCountForDiagnostics()
	);
	AddLease(
		Snapshot,
		TEXT("KeepScreenAwake"),
		FOpenMobileDeviceKeepScreenAwakeControlService::GetActiveRequestCountForDiagnostics()
	);
	AddLease(
		Snapshot,
		TEXT("SystemUi"),
		FOpenMobileDeviceSystemUiControlService::GetActiveRequestCountForDiagnostics()
	);
	AddLease(
		Snapshot,
		TEXT("PreferredRefreshRate"),
		FOpenMobileDeviceRefreshRateControlService::GetActiveRequestCountForDiagnostics()
	);
	AddLease(
		Snapshot,
		TEXT("Orientation"),
		FOpenMobileDeviceOrientationControlService::GetActiveRequestCountForDiagnostics()
	);
	Snapshot.RecentErrors = RecentErrors;
	Snapshot.ConfigurationIssues = CaptureConfigurationIssues();
	return Snapshot;
}

void FOpenMobileDeviceDiagnosticsSource::RecordError(
	FName Operation,
	const FOpenMobileError& Error
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceDiagnosticsSourcePrivate;
	if (!Error.IsSet())
	{
		return;
	}
	if (RecentErrors.Num() >= MaximumRecentErrorCount)
	{
		RecentErrors.RemoveAt(0);
	}
	RecentErrors.Add({
		FDateTime::UtcNow(),
		Operation.IsNone() ? FName(TEXT("DeviceOperation")) : Operation,
		Error.Code
	});
}

int32 FOpenMobileDeviceDiagnosticsSource::GetMaximumCapabilityCount()
{
	return OpenMobileDeviceDiagnosticsSourcePrivate::MaximumCapabilityCount;
}

int32 FOpenMobileDeviceDiagnosticsSource::GetMaximumRecentErrorCount()
{
	return OpenMobileDeviceDiagnosticsSourcePrivate::MaximumRecentErrorCount;
}

int32 FOpenMobileDeviceDiagnosticsSource::GetMaximumConfigurationIssueCount()
{
	return OpenMobileDeviceDiagnosticsSourcePrivate::MaximumConfigurationIssueCount;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceDiagnosticsSource::ResetForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceDiagnosticsSourcePrivate::RecentErrors.Reset();
}
#endif

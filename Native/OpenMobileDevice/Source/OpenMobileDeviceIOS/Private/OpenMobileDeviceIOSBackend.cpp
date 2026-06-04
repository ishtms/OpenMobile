#include "OpenMobileDeviceIOSBackend.h"

#include "HAL/PlatformMisc.h"
#include "OpenMobileDeviceArchitecture.h"
#include "OpenMobileDeviceIOSApplication.h"
#include "OpenMobileDeviceIOSBattery.h"
#include "OpenMobileDeviceIOSDisplay.h"
#include "OpenMobileDeviceIOSIdentity.h"
#include "OpenMobileDeviceIOSLocale.h"
#include "OpenMobileDeviceIOSLocaleMonitor.h"
#include "OpenMobileDeviceIOSMemory.h"
#include "OpenMobileDeviceIOSMemoryMonitor.h"
#include "OpenMobileDeviceIOSNetwork.h"
#include "OpenMobileDeviceIOSNetworkMonitor.h"
#include "OpenMobileDeviceIOSOrientationControl.h"
#include "OpenMobileDeviceIOSStorage.h"
#include "OpenMobileDevicePlatformInfo.h"
#include "OpenMobileDeviceProcessorInfo.h"

#include <TargetConditionals.h>

FOpenMobileCapability FOpenMobileDeviceIOSBackend::GetDomainCapability(
	EOpenMobileDeviceBackendDomain Domain
) const
{
	FOpenMobileCapability Capability;
	Capability.Name = GetDomainCapabilityName(Domain);
	Capability.State = Domain == EOpenMobileDeviceBackendDomain::Identity
		|| Domain == EOpenMobileDeviceBackendDomain::Environment
		|| Domain == EOpenMobileDeviceBackendDomain::Power
		|| Domain == EOpenMobileDeviceBackendDomain::Connectivity
		|| Domain == EOpenMobileDeviceBackendDomain::Display
		|| Domain == EOpenMobileDeviceBackendDomain::Utility
		? EOpenMobileCapabilityState::Available
		: EOpenMobileCapabilityState::NotSupported;
	return Capability;
}

FOpenMobileDeviceCapability FOpenMobileDeviceIOSBackend::GetCapability(
	FName CapabilityName
) const
{
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::WindowMetrics)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS reports the active Unreal view, drawable size, native scale, current screen, and windowed state. Physical DPI is unavailable.");
		return Capability;
	}
	if (CapabilityName
		== FOpenMobileDeviceCapabilityNames::RefreshRateInformation)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS exposes the attached screen's maximum frame rate. UIKit does not expose an instantaneous effective rate or a supported refresh-mode catalog.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::RefreshRateControl)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS refresh preferences are tied to the engine-owned display link. OpenMobile does not alter Unreal frame pacing, VSync, or game timing.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::SafeAreaInsets)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS reports active-view safe-area insets, the scene status-bar intersection, and the bottom home-indicator reservation in logical points. It does not expose separate numeric system-gesture insets.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::DisplayCutout)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS does not expose display-cutout rectangles or waterfall edge geometry. Use the active view's safe-area and home-indicator insets for layout.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::WindowOrientation
		|| CapabilityName
			== FOpenMobileDeviceCapabilityNames::WindowChangeEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS reports the active UIWindowScene interface orientation. Safe-frame changes and bounded Window Display monitoring refresh the complete snapshot after layout settles.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::OrientationControl)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS updates Unreal's active view-controller orientation mask and submits a scene geometry request. Project and presented-controller restrictions remain authoritative.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::MultiWindowEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS reports full-screen geometry directly. Public UIKit geometry does not reliably distinguish Split View, Slide Over, Stage Manager, or other windowed arrangements, so those modes remain Unknown.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::FoldablePosture)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS does not expose public foldable posture or hinge geometry. OpenMobile does not infer posture from screen dimensions.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::HdrWideColor)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.BackendName = GetBackendName();
#if TARGET_OS_SIMULATOR
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.Limit = EOpenMobileDeviceCapabilityLimit::Simulator;
		Capability.Detail = TEXT("iOS Simulator does not represent physical display HDR or wide-color capability.");
#else
		if (FPlatformMisc::IOSVersionCompare(10, 0, 0))
		{
			Capability.State = EOpenMobileCapabilityState::Available;
			Capability.Detail = TEXT("iOS reports active-screen P3 gamut on iOS 10 or newer and potential EDR headroom on iOS 16 or newer. UIKit does not expose a supported HDR-format catalog. Unreal's current HDR output state is reported separately when the RHI is initialized.");
		}
		else
		{
			Capability.State = EOpenMobileCapabilityState::NotSupported;
			Capability.Limit =
				EOpenMobileDeviceCapabilityLimit::MinimumOsVersion;
			Capability.MinimumOsVersion =
				FOpenMobileDeviceOptionalString::MakeAvailable(
					TEXT("iOS 10")
				);
		}
#endif
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::MemoryPressureEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS memory-warning notifications are advisory and do not report an exact free-memory threshold.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::StorageSpace)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.BackendName = GetBackendName();
#if TARGET_OS_SIMULATOR
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.Limit = EOpenMobileDeviceCapabilityLimit::Simulator;
		Capability.Detail = TEXT("iOS Simulator storage would describe the host Mac volume.");
#else
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.Detail = TEXT("iOS reports the application data volume, including distinct important-usage capacity where available.");
#endif
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::LowStorageEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.BackendName = GetBackendName();
#if TARGET_OS_SIMULATOR
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.Limit = EOpenMobileDeviceCapabilityLimit::Simulator;
		Capability.Detail = TEXT("iOS Simulator storage would describe the host Mac volume.");
#else
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.Detail = TEXT("iOS has no public low-storage notification for this threshold. Checks run only while storage monitoring is requested and use the configured bounded interval.");
#endif
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::NetworkPath)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS reports whether the process has a usable local route. This does not prove that any remote endpoint can answer.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::NetworkChangeEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS uses Unreal's process-level default-path monitor for demand-driven events.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::EndpointReachability)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("Explicit HTTPS endpoint tests use platform TLS validation and never run periodically without a caller request.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::ThermalHeadroom)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS does not expose a public thermal-headroom or forecast API.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::ThermalState
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::ThermalEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.BackendName = GetBackendName();
#if TARGET_OS_SIMULATOR
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.Limit = EOpenMobileDeviceCapabilityLimit::Simulator;
		Capability.Detail = TEXT("iOS Simulator does not provide device thermal state.");
#else
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.Detail = TEXT("iOS thermal state is advisory and available on iOS 11 or newer.");
#endif
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::PowerSavingMode
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::PowerSavingEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.BackendName = GetBackendName();
#if TARGET_OS_SIMULATOR
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.Limit = EOpenMobileDeviceCapabilityLimit::Simulator;
		Capability.Detail = TEXT("iOS Simulator does not provide device Low Power Mode state.");
#else
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.Detail = TEXT("iOS Low Power Mode is available on iOS 9 or newer.");
#endif
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::ChargingSource)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("iOS does not expose a public charging-source API.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::PlatformInformation
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::ManufacturerBrandModel
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::HardwareModelIdentifier
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::FormFactor
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::CpuArchitecture
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::LogicalProcessorCount
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::PhysicalMemory
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::ApplicationMetadata
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::EmulatorDetection
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::PreferredLanguages
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::Locale
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::TimeZone
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::RegionalFormatting
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::LocaleChangeEvents
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::BatteryLevel
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::ChargingState
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::BatteryEvents
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::MediaVolume
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::VolumeEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		return Capability;
	}
	return IOpenMobileDeviceBackend::GetCapability(CapabilityName);
}

FOpenMobileWindowDisplaySnapshot
FOpenMobileDeviceIOSBackend::GetWindowDisplaySnapshot() const
{
	return GetOpenMobileDeviceIOSWindowDisplaySnapshot();
}

FOpenMobileOrientationPolicyResult
FOpenMobileDeviceIOSBackend::ApplyOrientationPolicy(
	const FOpenMobileOrientationPolicyRequest& Request
)
{
	return ApplyOpenMobileDeviceIOSOrientationPolicy(Request);
}

void FOpenMobileDeviceIOSBackend::ClearOrientationPolicy()
{
	ClearOpenMobileDeviceIOSOrientationPolicy();
}

FOpenMobileDeviceInformationSnapshot
FOpenMobileDeviceIOSBackend::GetDeviceInformationSnapshot() const
{
	FOpenMobileDeviceInformationSnapshot Snapshot =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
		EOpenMobileDevicePlatform::IOS,
		FPlatformMisc::GetOSVersion(),
		0,
		FString(),
		FString(),
		GetOpenMobileDeviceIOSModel(),
		GetOpenMobileDeviceIOSHardwareModel()
	);
	FOpenMobileDeviceArchitecture::Apply(
		Snapshot,
		FPlatformMisc::GetUBTArchitecture(),
		{},
		false
	);
	FOpenMobileDeviceProcessorInfo::ApplyLogicalProcessorCount(
		Snapshot,
		FPlatformMisc::NumberOfCoresIncludingHyperthreads()
	);
	ApplyOpenMobileDeviceIOSEmulatorDetection(Snapshot);
	return Snapshot;
}

EOpenMobileDeviceFormFactor
FOpenMobileDeviceIOSBackend::GetDeviceFormFactor() const
{
	return GetOpenMobileDeviceIOSFormFactor();
}

FOpenMobileApplicationMetadataSnapshot
FOpenMobileDeviceIOSBackend::GetApplicationMetadataSnapshot() const
{
	return GetOpenMobileDeviceIOSApplicationMetadata();
}

FOpenMobileLocaleSnapshot FOpenMobileDeviceIOSBackend::GetLocaleSnapshot() const
{
	return GetLocaleSnapshotAtUtc(FDateTime::UtcNow());
}

FOpenMobileLocaleSnapshot FOpenMobileDeviceIOSBackend::GetLocaleSnapshotAtUtc(
	const FDateTime& UtcInstant
) const
{
	return GetOpenMobileDeviceIOSLocaleSnapshot(UtcInstant);
}

FOpenMobileMemorySnapshot FOpenMobileDeviceIOSBackend::GetMemorySnapshot() const
{
	FOpenMobileMemorySnapshot Snapshot = GetOpenMobileDeviceIOSMemorySnapshot();
	ApplyOpenMobileDeviceIOSMemoryPressureEvent(Snapshot);
	return Snapshot;
}

FOpenMobileNetworkPathSnapshot
FOpenMobileDeviceIOSBackend::GetNetworkPathSnapshot() const
{
	return GetOpenMobileDeviceIOSNetworkPathSnapshot();
}

FOpenMobilePowerSnapshot FOpenMobileDeviceIOSBackend::GetPowerSnapshot() const
{
	return GetOpenMobileDeviceIOSPowerSnapshot();
}

bool FOpenMobileDeviceIOSBackend::QueryStorageSnapshot(
	FOpenMobileStorageSnapshot& OutSnapshot,
	FOpenMobileError& OutError
) const
{
	return QueryOpenMobileDeviceIOSStorage(OutSnapshot, OutError);
}

int64 FOpenMobileDeviceIOSBackend::GetPlatformLowStorageThresholdBytes(
	const FOpenMobileStorageSnapshot& Snapshot
) const
{
	constexpr int64 MaximumThresholdBytes = 1024ll * 1024 * 1024;
	return Snapshot.TotalBytes.bIsAvailable
		? FMath::Min(Snapshot.TotalBytes.Value / 20, MaximumThresholdBytes)
		: MaximumThresholdBytes;
}

FOpenMobileMediaVolumeSnapshot
FOpenMobileDeviceIOSBackend::GetMediaVolumeSnapshot() const
{
	FOpenMobileMediaVolumeSnapshot Snapshot;
	const int32 VolumePercent = FPlatformMisc::GetDeviceVolume();
	if (VolumePercent >= 0)
	{
		Snapshot.VolumePercent = FOpenMobileDeviceOptionalFloat::MakeAvailable(
			FMath::Clamp(static_cast<float>(VolumePercent), 0.0f, 100.0f)
		);
	}
	return Snapshot;
}

bool FOpenMobileDeviceIOSBackend::StartMonitoring(
	EOpenMobileDeviceMonitoringGroup Group,
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	if (Group == EOpenMobileDeviceMonitoringGroup::Locale)
	{
		return StartOpenMobileDeviceIOSLocaleMonitoring(CallbackToken);
	}
	if (Group == EOpenMobileDeviceMonitoringGroup::Power)
	{
		return StartOpenMobileDeviceIOSBatteryMonitoring(CallbackToken);
	}
	if (Group == EOpenMobileDeviceMonitoringGroup::MemoryPressure)
	{
		return StartOpenMobileDeviceIOSMemoryMonitoring(CallbackToken);
	}
	if (Group == EOpenMobileDeviceMonitoringGroup::Network)
	{
		return StartOpenMobileDeviceIOSNetworkMonitoring(CallbackToken);
	}
	return false;
}

void FOpenMobileDeviceIOSBackend::StopMonitoring(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	if (Group == EOpenMobileDeviceMonitoringGroup::Locale)
	{
		StopOpenMobileDeviceIOSLocaleMonitoring();
	}
	else if (Group == EOpenMobileDeviceMonitoringGroup::Power)
	{
		StopOpenMobileDeviceIOSBatteryMonitoring();
	}
	else if (Group == EOpenMobileDeviceMonitoringGroup::MemoryPressure)
	{
		StopOpenMobileDeviceIOSMemoryMonitoring();
	}
	else if (Group == EOpenMobileDeviceMonitoringGroup::Network)
	{
		StopOpenMobileDeviceIOSNetworkMonitoring();
	}
}

void FOpenMobileDeviceIOSBackend::BeginShutdown()
{
	ClearOpenMobileDeviceIOSOrientationPolicy();
	StopOpenMobileDeviceIOSLocaleMonitoring();
	StopOpenMobileDeviceIOSBatteryMonitoring();
	StopOpenMobileDeviceIOSMemoryMonitoring();
	StopOpenMobileDeviceIOSNetworkMonitoring();
}

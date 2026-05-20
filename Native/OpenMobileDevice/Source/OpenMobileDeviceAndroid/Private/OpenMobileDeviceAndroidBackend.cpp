#include "OpenMobileDeviceAndroidBackend.h"

#include "Android/AndroidPlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "OpenMobileDeviceArchitecture.h"
#include "OpenMobileDeviceAndroidApplication.h"
#include "OpenMobileDeviceAndroidBattery.h"
#include "OpenMobileDeviceAndroidDisplay.h"
#include "OpenMobileDeviceAndroidIdentity.h"
#include "OpenMobileDeviceAndroidLocale.h"
#include "OpenMobileDeviceAndroidLocaleMonitor.h"
#include "OpenMobileDeviceAndroidMemoryMonitor.h"
#include "OpenMobileDeviceAndroidNetwork.h"
#include "OpenMobileDeviceAndroidNetworkMonitor.h"
#include "OpenMobileDeviceAndroidStorage.h"
#include "OpenMobileDeviceAndroidStorageMonitor.h"
#include "OpenMobileDeviceMemoryInfo.h"
#include "OpenMobileDevicePlatformInfo.h"
#include "OpenMobileDeviceProcessorInfo.h"
#include "OpenMobileDeviceSettings.h"

FOpenMobileCapability FOpenMobileDeviceAndroidBackend::GetDomainCapability(
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

FOpenMobileDeviceCapability FOpenMobileDeviceAndroidBackend::GetCapability(
	FName CapabilityName
) const
{
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::WindowMetrics)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("Android reports the active native window, drawable surface, density, display, and multi-window state without using physical panel dimensions.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::MemoryPressureEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("Android memory events use ComponentCallbacks2 trim hints. Running-pressure levels deprecated in API 35 may be absent on current systems; byte estimates remain independent.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::StorageSpace)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("Android reports the internal application data volume. Shared and removable storage are outside this query.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::LowStorageEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("Android system low-storage and recovery broadcasts refresh the application data volume. Custom thresholds add bounded demand-driven checks.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::NetworkPath)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("Android distinguishes a declared Internet capability from an OS-validated default path. The snapshot performs no endpoint probe.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::NetworkChangeEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = FAndroidMisc::GetAndroidBuildVersion() >= 24
			? TEXT("Android default-network callbacks provide demand-driven path events.")
			: TEXT("Android versions before API 24 use demand-driven fallback checks.");
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
		Capability.BackendName = GetBackendName();
		if (FAndroidMisc::GetAndroidBuildVersion() >= 30)
		{
			Capability.State = EOpenMobileCapabilityState::Available;
			Capability.Detail = TEXT("Android thermal headroom accepts forecast windows from 0 through 60 seconds on API 30 or newer. OpenMobile requests a 10-second forecast no more than once every 10 seconds; unsupported devices return unavailable data.");
		}
		else
		{
			Capability.State = EOpenMobileCapabilityState::NotSupported;
			Capability.Limit = EOpenMobileDeviceCapabilityLimit::MinimumOsVersion;
			Capability.MinimumOsVersion =
				FOpenMobileDeviceOptionalString::MakeAvailable(
					TEXT("Android 11 (API 30)")
				);
		}
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::ThermalState
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::ThermalEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.BackendName = GetBackendName();
		if (FAndroidMisc::GetAndroidBuildVersion() >= 29)
		{
			Capability.State = EOpenMobileCapabilityState::Available;
			Capability.Detail = TEXT("Android thermal status is advisory and available on API 29 or newer.");
		}
		else
		{
			Capability.State = EOpenMobileCapabilityState::NotSupported;
			Capability.Limit = EOpenMobileDeviceCapabilityLimit::MinimumOsVersion;
			Capability.MinimumOsVersion =
				FOpenMobileDeviceOptionalString::MakeAvailable(
					TEXT("Android 10 (API 29)")
				);
		}
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::PowerSavingMode
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::PowerSavingEvents)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("Android power-save mode is available on API 21 or newer.");
		return Capability;
	}
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::ChargingSource)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.BackendName = GetBackendName();
		Capability.Detail = TEXT("AC and USB are available on every supported Android version. Wireless requires API 17. Dock maps to Other on API 33 or newer.");
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
FOpenMobileDeviceAndroidBackend::GetWindowDisplaySnapshot() const
{
	return GetOpenMobileDeviceAndroidWindowDisplaySnapshot();
}

FOpenMobileDeviceInformationSnapshot
FOpenMobileDeviceAndroidBackend::GetDeviceInformationSnapshot() const
{
	FOpenMobileDeviceInformationSnapshot Snapshot =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
		EOpenMobileDevicePlatform::Android,
		FPlatformMisc::GetOSVersion(),
		FAndroidMisc::GetAndroidBuildVersion(),
		FAndroidMisc::GetDeviceMake(),
		GetOpenMobileDeviceAndroidBrand(),
		FAndroidMisc::GetDeviceModel(),
		GetOpenMobileDeviceAndroidHardwareModel()
	);
	TArray<FString> SupportedAbis;
	const bool bSupportedAbisAvailable =
		GetOpenMobileDeviceAndroidSupportedAbis(SupportedAbis);
	FOpenMobileDeviceArchitecture::Apply(
		Snapshot,
		FPlatformMisc::GetUBTArchitecture(),
		SupportedAbis,
		bSupportedAbisAvailable
	);
	FOpenMobileDeviceProcessorInfo::ApplyLogicalProcessorCount(
		Snapshot,
		FPlatformMisc::NumberOfCoresIncludingHyperthreads()
	);
	ApplyOpenMobileDeviceAndroidEmulatorDetection(Snapshot);
	return Snapshot;
}

EOpenMobileDeviceFormFactor
FOpenMobileDeviceAndroidBackend::GetDeviceFormFactor() const
{
	return GetOpenMobileDeviceAndroidFormFactor();
}

FOpenMobileApplicationMetadataSnapshot
FOpenMobileDeviceAndroidBackend::GetApplicationMetadataSnapshot() const
{
	return GetOpenMobileDeviceAndroidApplicationMetadata();
}

FOpenMobileLocaleSnapshot
FOpenMobileDeviceAndroidBackend::GetLocaleSnapshot() const
{
	return GetLocaleSnapshotAtUtc(FDateTime::UtcNow());
}

FOpenMobileLocaleSnapshot
FOpenMobileDeviceAndroidBackend::GetLocaleSnapshotAtUtc(
	const FDateTime& UtcInstant
) const
{
	return GetOpenMobileDeviceAndroidLocaleSnapshot(UtcInstant);
}

FOpenMobileMemorySnapshot
FOpenMobileDeviceAndroidBackend::GetMemorySnapshot() const
{
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	EOpenMobileMemoryPressureState PressureState =
		EOpenMobileMemoryPressureState::Unknown;
	switch (Stats.GetMemoryPressureStatus())
	{
	case FPlatformMemoryStats::EMemoryPressureStatus::Nominal:
		PressureState = EOpenMobileMemoryPressureState::Nominal;
		break;
	case FPlatformMemoryStats::EMemoryPressureStatus::Warning:
		PressureState = EOpenMobileMemoryPressureState::Warning;
		break;
	case FPlatformMemoryStats::EMemoryPressureStatus::Critical:
		PressureState = EOpenMobileMemoryPressureState::Critical;
		break;
	case FPlatformMemoryStats::EMemoryPressureStatus::Unknown:
		break;
	}
	FOpenMobileMemorySnapshot Snapshot = FOpenMobileDeviceMemoryInfo::Build(
		Stats.TotalPhysical,
		Stats.AvailablePhysical,
		false,
		PressureState
	);
	ApplyOpenMobileDeviceAndroidMemoryPressureEvent(Snapshot);
	return Snapshot;
}

FOpenMobileNetworkPathSnapshot
FOpenMobileDeviceAndroidBackend::GetNetworkPathSnapshot() const
{
	return GetOpenMobileDeviceAndroidNetworkPathSnapshot();
}

FOpenMobilePowerSnapshot
FOpenMobileDeviceAndroidBackend::GetPowerSnapshot() const
{
	return GetOpenMobileDeviceAndroidPowerSnapshot();
}

bool FOpenMobileDeviceAndroidBackend::QueryStorageSnapshot(
	FOpenMobileStorageSnapshot& OutSnapshot,
	FOpenMobileError& OutError
) const
{
	return QueryOpenMobileDeviceAndroidStorage(OutSnapshot, OutError);
}

int64 FOpenMobileDeviceAndroidBackend::GetPlatformLowStorageThresholdBytes(
	const FOpenMobileStorageSnapshot& Snapshot
) const
{
	constexpr int64 MaximumThresholdBytes = 500ll * 1024 * 1024;
	return Snapshot.TotalBytes.bIsAvailable
		? FMath::Min(Snapshot.TotalBytes.Value / 20, MaximumThresholdBytes)
		: MaximumThresholdBytes;
}

FOpenMobileMediaVolumeSnapshot
FOpenMobileDeviceAndroidBackend::GetMediaVolumeSnapshot() const
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

bool FOpenMobileDeviceAndroidBackend::StartMonitoring(
	EOpenMobileDeviceMonitoringGroup Group,
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	if (Group == EOpenMobileDeviceMonitoringGroup::Locale)
	{
		return StartOpenMobileDeviceAndroidLocaleMonitoring(CallbackToken);
	}
	if (Group == EOpenMobileDeviceMonitoringGroup::Power)
	{
		return StartOpenMobileDeviceAndroidBatteryMonitoring(CallbackToken);
	}
	if (Group == EOpenMobileDeviceMonitoringGroup::MemoryPressure)
	{
		return StartOpenMobileDeviceAndroidMemoryMonitoring(CallbackToken);
	}
	if (Group == EOpenMobileDeviceMonitoringGroup::Storage)
	{
		return StartOpenMobileDeviceAndroidStorageMonitoring(CallbackToken);
	}
	if (Group == EOpenMobileDeviceMonitoringGroup::Network)
	{
		return StartOpenMobileDeviceAndroidNetworkMonitoring(CallbackToken);
	}
	return false;
}

void FOpenMobileDeviceAndroidBackend::StopMonitoring(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	if (Group == EOpenMobileDeviceMonitoringGroup::Locale)
	{
		StopOpenMobileDeviceAndroidLocaleMonitoring();
	}
	else if (Group == EOpenMobileDeviceMonitoringGroup::Power)
	{
		StopOpenMobileDeviceAndroidBatteryMonitoring();
	}
	else if (Group == EOpenMobileDeviceMonitoringGroup::MemoryPressure)
	{
		StopOpenMobileDeviceAndroidMemoryMonitoring();
	}
	else if (Group == EOpenMobileDeviceMonitoringGroup::Storage)
	{
		StopOpenMobileDeviceAndroidStorageMonitoring();
	}
	else if (Group == EOpenMobileDeviceMonitoringGroup::Network)
	{
		StopOpenMobileDeviceAndroidNetworkMonitoring();
	}
}

bool FOpenMobileDeviceAndroidBackend::RequiresFallbackPolling(
	EOpenMobileDeviceMonitoringGroup Group
) const
{
	return Group == EOpenMobileDeviceMonitoringGroup::Storage
		&& (!GetDefault<UOpenMobileDeviceSettings>()
				->bUsePlatformDefaultLowStorageThreshold
			|| GetDefault<UOpenMobileDeviceSettings>()
				->GetValidatedLowStorageRecoveryHysteresisBytes() > 0);
}

void FOpenMobileDeviceAndroidBackend::BeginShutdown()
{
	StopOpenMobileDeviceAndroidLocaleMonitoring();
	StopOpenMobileDeviceAndroidBatteryMonitoring();
	StopOpenMobileDeviceAndroidMemoryMonitoring();
	StopOpenMobileDeviceAndroidStorageMonitoring();
	StopOpenMobileDeviceAndroidNetworkMonitoring();
}

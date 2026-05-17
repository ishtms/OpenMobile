#include "OpenMobileDeviceAndroidBackend.h"

#include "Android/AndroidPlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "OpenMobileDeviceArchitecture.h"
#include "OpenMobileDeviceAndroidApplication.h"
#include "OpenMobileDeviceAndroidBattery.h"
#include "OpenMobileDeviceAndroidIdentity.h"
#include "OpenMobileDeviceAndroidLocale.h"
#include "OpenMobileDeviceAndroidLocaleMonitor.h"
#include "OpenMobileDeviceMemoryInfo.h"
#include "OpenMobileDevicePlatformInfo.h"
#include "OpenMobileDeviceProcessorInfo.h"

FOpenMobileCapability FOpenMobileDeviceAndroidBackend::GetDomainCapability(
	EOpenMobileDeviceBackendDomain Domain
) const
{
	FOpenMobileCapability Capability;
	Capability.Name = GetDomainCapabilityName(Domain);
	Capability.State = Domain == EOpenMobileDeviceBackendDomain::Identity
		|| Domain == EOpenMobileDeviceBackendDomain::Environment
		|| Domain == EOpenMobileDeviceBackendDomain::Power
		|| Domain == EOpenMobileDeviceBackendDomain::Utility
		? EOpenMobileCapabilityState::Available
		: EOpenMobileCapabilityState::NotSupported;
	return Capability;
}

FOpenMobileDeviceCapability FOpenMobileDeviceAndroidBackend::GetCapability(
	FName CapabilityName
) const
{
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::ThermalState)
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
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::PowerSavingMode)
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
	return FOpenMobileDeviceMemoryInfo::Build(
		Stats.TotalPhysical,
		Stats.AvailablePhysical,
		false,
		PressureState
	);
}

FOpenMobilePowerSnapshot
FOpenMobileDeviceAndroidBackend::GetPowerSnapshot() const
{
	return GetOpenMobileDeviceAndroidPowerSnapshot();
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
}

void FOpenMobileDeviceAndroidBackend::BeginShutdown()
{
	StopOpenMobileDeviceAndroidLocaleMonitoring();
	StopOpenMobileDeviceAndroidBatteryMonitoring();
}

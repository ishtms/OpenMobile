#include "OpenMobileDeviceAndroidBackend.h"

#include "Android/AndroidPlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "OpenMobileDeviceArchitecture.h"
#include "OpenMobileDeviceAndroidIdentity.h"
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
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::PlatformInformation
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::ManufacturerBrandModel
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::HardwareModelIdentifier
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::FormFactor
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::CpuArchitecture
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::LogicalProcessorCount
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::PhysicalMemory
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::BatteryLevel
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
	return Snapshot;
}

EOpenMobileDeviceFormFactor
FOpenMobileDeviceAndroidBackend::GetDeviceFormFactor() const
{
	return GetOpenMobileDeviceAndroidFormFactor();
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
	FOpenMobilePowerSnapshot Snapshot;
	const int32 BatteryPercent = FPlatformMisc::GetBatteryLevel();
	if (BatteryPercent >= 0)
	{
		Snapshot.BatteryPercent = FOpenMobileDeviceOptionalFloat::MakeAvailable(
			FMath::Clamp(static_cast<float>(BatteryPercent), 0.0f, 100.0f)
		);
	}
	return Snapshot;
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

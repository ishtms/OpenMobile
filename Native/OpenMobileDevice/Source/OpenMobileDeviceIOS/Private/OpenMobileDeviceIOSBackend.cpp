#include "OpenMobileDeviceIOSBackend.h"

#include "HAL/PlatformMisc.h"
#include "OpenMobileDevicePlatformInfo.h"

FOpenMobileCapability FOpenMobileDeviceIOSBackend::GetDomainCapability(
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

FOpenMobileDeviceCapability FOpenMobileDeviceIOSBackend::GetCapability(
	FName CapabilityName
) const
{
	if (CapabilityName == FOpenMobileDeviceCapabilityNames::PlatformInformation
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
FOpenMobileDeviceIOSBackend::GetDeviceInformationSnapshot() const
{
	return FOpenMobileDevicePlatformInfo::BuildSnapshot(
		EOpenMobileDevicePlatform::IOS,
		FPlatformMisc::GetOSVersion(),
		0
	);
}

FOpenMobilePowerSnapshot FOpenMobileDeviceIOSBackend::GetPowerSnapshot() const
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

#include "OpenMobileDeviceIOSBackend.h"

#include "HAL/PlatformMisc.h"
#include "OpenMobileDeviceArchitecture.h"
#include "OpenMobileDeviceIOSApplication.h"
#include "OpenMobileDeviceIOSBattery.h"
#include "OpenMobileDeviceIOSIdentity.h"
#include "OpenMobileDeviceIOSLocale.h"
#include "OpenMobileDeviceIOSLocaleMonitor.h"
#include "OpenMobileDeviceIOSMemory.h"
#include "OpenMobileDevicePlatformInfo.h"
#include "OpenMobileDeviceProcessorInfo.h"

FOpenMobileCapability FOpenMobileDeviceIOSBackend::GetDomainCapability(
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

FOpenMobileDeviceCapability FOpenMobileDeviceIOSBackend::GetCapability(
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
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::ApplicationMetadata
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::EmulatorDetection
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::PreferredLanguages
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::Locale
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::TimeZone
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::RegionalFormatting
		|| CapabilityName == FOpenMobileDeviceCapabilityNames::LocaleChangeEvents
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
	return GetOpenMobileDeviceIOSMemorySnapshot();
}

FOpenMobilePowerSnapshot FOpenMobileDeviceIOSBackend::GetPowerSnapshot() const
{
	return GetOpenMobileDeviceIOSPowerSnapshot();
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
}

void FOpenMobileDeviceIOSBackend::BeginShutdown()
{
	StopOpenMobileDeviceIOSLocaleMonitoring();
	StopOpenMobileDeviceIOSBatteryMonitoring();
}

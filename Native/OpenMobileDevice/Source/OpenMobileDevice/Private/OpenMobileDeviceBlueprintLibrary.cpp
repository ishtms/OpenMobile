#include "OpenMobileDeviceBlueprintLibrary.h"

#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceSnapshotService.h"

namespace OpenMobileDeviceBlueprintLibraryPrivate
{
	int64 CapabilityReportGeneration = 0;

	int32 ToLegacyPercent(const FOpenMobileDeviceOptionalFloat& Value)
	{
		return Value.bIsAvailable
			? FMath::Clamp(FMath::RoundToInt(Value.Value), 0, 100)
			: -1;
	}

	FOpenMobileDeviceStatus GetLegacyStatus()
	{
		FOpenMobileDeviceStatus Status;
		Status.BatteryPercent = ToLegacyPercent(
			FOpenMobileDeviceSnapshotService::GetPowerSnapshot().BatteryPercent
		);
		Status.VolumePercent = ToLegacyPercent(
			FOpenMobileDeviceSnapshotService::GetMediaVolumeSnapshot().VolumePercent
		);
		Status.bBatteryAvailable = Status.BatteryPercent >= 0;
		Status.bVolumeAvailable = Status.VolumePercent >= 0;
		return Status;
	}

	FOpenMobileDeviceCapability QueryKnownCapability(FName CapabilityName)
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		if (CapabilityName == FOpenMobileDeviceCapabilityNames::CapabilityReport)
		{
			Capability.State = EOpenMobileCapabilityState::Available;
			Capability.BackendName = TEXT("OpenMobileDevice");
			return Capability;
		}

		if (FOpenMobileDeviceBackendRegistry::IsShuttingDown())
		{
			Capability.State = EOpenMobileCapabilityState::TemporarilyUnavailable;
			Capability.Limit = EOpenMobileDeviceCapabilityLimit::NativeResourceConflict;
			Capability.Detail = TEXT("The Device service is shutting down.");
			return Capability;
		}

		IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend();
		if (!Backend)
		{
			Capability.State = EOpenMobileCapabilityState::NotSupported;
			Capability.Limit = EOpenMobileDeviceCapabilityLimit::UnsupportedPlatform;
			Capability.Detail = TEXT("No Device backend is available on this platform.");
			return Capability;
		}

		Capability = Backend->GetCapability(CapabilityName);
		Capability.Name = CapabilityName;
		Capability.State = FOpenMobileDeviceCapability::NormalizeState(Capability.State);
		if (Capability.BackendName.IsNone())
		{
			Capability.BackendName = Backend->GetBackendName();
		}
		if (Capability.State == EOpenMobileCapabilityState::Available)
		{
			Capability.Limit = EOpenMobileDeviceCapabilityLimit::None;
			Capability.MinimumOsVersion = {};
		}
		return Capability;
	}
}

int32 UOpenMobileDeviceBlueprintLibrary::GetBatteryPercent()
{
	return OpenMobileDeviceBlueprintLibraryPrivate::ToLegacyPercent(
		FOpenMobileDeviceSnapshotService::GetPowerSnapshot().BatteryPercent
	);
}

int32 UOpenMobileDeviceBlueprintLibrary::GetVolumePercent()
{
	return OpenMobileDeviceBlueprintLibraryPrivate::ToLegacyPercent(
		FOpenMobileDeviceSnapshotService::GetMediaVolumeSnapshot().VolumePercent
	);
}

FOpenMobileDeviceStatus UOpenMobileDeviceBlueprintLibrary::GetDeviceStatus()
{
	return OpenMobileDeviceBlueprintLibraryPrivate::GetLegacyStatus();
}

FOpenMobileDeviceCapability UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
	FName CapabilityName
)
{
	check(IsInGameThread());
	if (!FOpenMobileDeviceCapabilityNames::IsKnown(CapabilityName))
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::NotConfigured;
		Capability.Detail = TEXT("The capability name is not part of the Device contract.");
		return Capability;
	}
	return OpenMobileDeviceBlueprintLibraryPrivate::QueryKnownCapability(CapabilityName);
}

FOpenMobileDeviceCapabilityReport
UOpenMobileDeviceBlueprintLibrary::GetDeviceCapabilityReport()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBlueprintLibraryPrivate;
	FOpenMobileDeviceCapabilityReport Report;
	Report.Metadata.CapturedAtUtc = FDateTime::UtcNow();
	if (CapabilityReportGeneration == MAX_int64)
	{
		CapabilityReportGeneration = 1;
	}
	else
	{
		++CapabilityReportGeneration;
	}
	Report.Metadata.Generation = CapabilityReportGeneration;
	const TArray<FName>& Names = FOpenMobileDeviceCapabilityNames::GetAll();
	Report.Capabilities.Reserve(Names.Num());
	for (const FName Name : Names)
	{
		Report.Capabilities.Add(QueryKnownCapability(Name));
	}
	return Report;
}

FText UOpenMobileDeviceBlueprintLibrary::FormatByteCount(int64 Bytes)
{
	return Bytes < 0
		? FText::GetEmpty()
		: FText::AsMemory(
			static_cast<uint64>(Bytes),
			EMemoryUnitStandard::IEC
		);
}

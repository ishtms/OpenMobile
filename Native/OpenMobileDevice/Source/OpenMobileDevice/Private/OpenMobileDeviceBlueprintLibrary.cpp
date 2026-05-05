#include "OpenMobileDeviceBlueprintLibrary.h"

#include "HAL/PlatformMisc.h"
#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileDeviceBackendRegistry.h"

namespace OpenMobileDeviceBlueprintLibraryPrivate
{
	int64 CapabilityReportGeneration = 0;

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
	const int32 Value = FPlatformMisc::GetBatteryLevel();
	return Value >= 0 ? FMath::Clamp(Value, 0, 100) : -1;
}

int32 UOpenMobileDeviceBlueprintLibrary::GetVolumePercent()
{
	const int32 Value = FPlatformMisc::GetDeviceVolume();
	return Value >= 0 ? FMath::Clamp(Value, 0, 100) : -1;
}

FOpenMobileDeviceStatus UOpenMobileDeviceBlueprintLibrary::GetDeviceStatus()
{
	FOpenMobileDeviceStatus Status;
	Status.BatteryPercent = GetBatteryPercent();
	Status.VolumePercent = GetVolumePercent();
	Status.bBatteryAvailable = Status.BatteryPercent >= 0;
	Status.bVolumeAvailable = Status.VolumePercent >= 0;
	return Status;
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

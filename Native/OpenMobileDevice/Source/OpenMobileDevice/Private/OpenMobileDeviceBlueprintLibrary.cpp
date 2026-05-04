#include "OpenMobileDeviceBlueprintLibrary.h"

#include "HAL/PlatformMisc.h"

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

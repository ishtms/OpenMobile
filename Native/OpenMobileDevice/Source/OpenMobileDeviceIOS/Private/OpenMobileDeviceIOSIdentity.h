#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceIdentityTypes.h"

FString GetOpenMobileDeviceIOSModel();
FString GetOpenMobileDeviceIOSHardwareModel();
EOpenMobileDeviceFormFactor GetOpenMobileDeviceIOSFormFactor();
void ApplyOpenMobileDeviceIOSEmulatorDetection(
	FOpenMobileDeviceInformationSnapshot& Snapshot
);

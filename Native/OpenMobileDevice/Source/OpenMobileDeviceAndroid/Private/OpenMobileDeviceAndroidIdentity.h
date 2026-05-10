#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceIdentityTypes.h"

FString GetOpenMobileDeviceAndroidBrand();
FString GetOpenMobileDeviceAndroidHardwareModel();
EOpenMobileDeviceFormFactor GetOpenMobileDeviceAndroidFormFactor();
bool GetOpenMobileDeviceAndroidSupportedAbis(TArray<FString>& OutSupportedAbis);

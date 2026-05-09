#pragma once

#include "OpenMobileDeviceIdentityTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDevicePlatformInfo final
{
public:
	static FOpenMobileDeviceInformationSnapshot BuildSnapshot(
		EOpenMobileDevicePlatform Platform,
		const FString& RawOsVersion,
		int32 AndroidApiLevel,
		const FString& Manufacturer = FString(),
		const FString& Brand = FString(),
		const FString& Model = FString()
	);
};

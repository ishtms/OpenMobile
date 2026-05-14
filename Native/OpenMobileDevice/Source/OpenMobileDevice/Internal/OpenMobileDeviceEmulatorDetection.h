#pragma once

#include "OpenMobileDeviceIdentityTypes.h"

struct FOpenMobileDeviceAndroidEmulatorEvidence
{
	FString Manufacturer;
	FString Brand;
	FString Model;
	FString Device;
	FString Hardware;
	FString Product;
	FString Fingerprint;
};

class OPENMOBILEDEVICE_API FOpenMobileDeviceEmulatorDetection final
{
public:
	static void ApplyAndroid(
		FOpenMobileDeviceInformationSnapshot& Snapshot,
		const FOpenMobileDeviceAndroidEmulatorEvidence& Evidence
	);
	static void ApplyIOS(
		FOpenMobileDeviceInformationSnapshot& Snapshot,
		bool bIsSimulator
	);
};

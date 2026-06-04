#pragma once

#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceBrightnessControl.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceLocaleTypes.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"

class FOpenMobileDeviceSnapshotService final
{
public:
	static FOpenMobileDeviceInformationSnapshot GetDeviceInformationSnapshot();
	static FOpenMobileApplicationMetadataSnapshot GetApplicationMetadataSnapshot();
	static FOpenMobileLocaleSnapshot GetLocaleSnapshot();
	static FOpenMobileLocaleSnapshot GetLocaleSnapshotAtUtc(
		const FDateTime& UtcInstant
	);
	static FOpenMobilePowerSnapshot GetPowerSnapshot();
	static FOpenMobileMediaVolumeSnapshot GetMediaVolumeSnapshot();
	static FOpenMobileMemorySnapshot GetMemorySnapshot();
	static FOpenMobileStorageSnapshot GetStorageSnapshot();
	static void StampStorageSnapshot(FOpenMobileStorageSnapshot& Snapshot);
	static FOpenMobileNetworkPathSnapshot GetNetworkPathSnapshot();
	static FOpenMobileWindowDisplaySnapshot GetWindowDisplaySnapshot();
	static FOpenMobileBrightnessSnapshot GetBrightnessSnapshot();
	static FOpenMobileAppearanceSnapshot GetAppearanceSnapshot();
	static FOpenMobileAccessibilitySnapshot GetAccessibilitySnapshot();
};

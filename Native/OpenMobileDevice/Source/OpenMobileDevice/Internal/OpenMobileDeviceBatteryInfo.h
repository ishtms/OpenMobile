#pragma once

#include "OpenMobileDeviceResourceTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceBatteryInfo final
{
public:
	static void ApplyFraction(
		FOpenMobilePowerSnapshot& Snapshot,
		double NativeLevel,
		bool bAvailable
	);
	static void ApplyRatio(
		FOpenMobilePowerSnapshot& Snapshot,
		int64 NativeLevel,
		int64 NativeScale,
		bool bAvailable
	);
};

#pragma once

#include "OpenMobileDeviceResourceTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceMemoryInfo final
{
public:
	static FOpenMobileMemorySnapshot Build(
		uint64 TotalPhysicalBytes,
		uint64 AvailablePhysicalBytes,
		bool bAvailableBytesAreApproximate,
		EOpenMobileMemoryPressureState PressureState
	);
};

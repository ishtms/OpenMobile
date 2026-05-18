#pragma once

#include "OpenMobileDeviceResourceTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceMemoryPressureInfo final
{
public:
	static EOpenMobileMemoryPressureState NormalizeAndroidTrimLevel(
		int32 NativeLevel
	);
	static EOpenMobileMemoryPressureState NormalizeIOSWarning();
};

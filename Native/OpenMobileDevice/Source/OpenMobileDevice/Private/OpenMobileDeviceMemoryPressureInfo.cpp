#include "OpenMobileDeviceMemoryPressureInfo.h"

EOpenMobileMemoryPressureState
FOpenMobileDeviceMemoryPressureInfo::NormalizeAndroidTrimLevel(
	int32 NativeLevel
)
{
	if (NativeLevel >= 80)
	{
		return EOpenMobileMemoryPressureState::Critical;
	}
	if (NativeLevel > 20)
	{
		return EOpenMobileMemoryPressureState::Warning;
	}
	if (NativeLevel == 20)
	{
		return EOpenMobileMemoryPressureState::Unknown;
	}
	if (NativeLevel >= 15)
	{
		return EOpenMobileMemoryPressureState::Critical;
	}
	if (NativeLevel >= 5)
	{
		return EOpenMobileMemoryPressureState::Warning;
	}
	return EOpenMobileMemoryPressureState::Unknown;
}

EOpenMobileMemoryPressureState
FOpenMobileDeviceMemoryPressureInfo::NormalizeIOSWarning()
{
	return EOpenMobileMemoryPressureState::Warning;
}

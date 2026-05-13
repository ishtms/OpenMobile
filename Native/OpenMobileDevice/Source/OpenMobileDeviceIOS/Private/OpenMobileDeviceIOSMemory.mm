#include "OpenMobileDeviceIOSMemory.h"

#include "HAL/PlatformMemory.h"
#include "OpenMobileDeviceMemoryInfo.h"

#include <os/proc.h>

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

FOpenMobileMemorySnapshot GetOpenMobileDeviceIOSMemorySnapshot()
{
#if TARGET_OS_SIMULATOR
	return {};
#else
	EOpenMobileMemoryPressureState PressureState =
		EOpenMobileMemoryPressureState::Unknown;
	switch (FPlatformMemory::GetStats().GetMemoryPressureStatus())
	{
	case FPlatformMemoryStats::EMemoryPressureStatus::Nominal:
		PressureState = EOpenMobileMemoryPressureState::Nominal;
		break;
	case FPlatformMemoryStats::EMemoryPressureStatus::Warning:
		PressureState = EOpenMobileMemoryPressureState::Warning;
		break;
	case FPlatformMemoryStats::EMemoryPressureStatus::Critical:
		PressureState = EOpenMobileMemoryPressureState::Critical;
		break;
	case FPlatformMemoryStats::EMemoryPressureStatus::Unknown:
		break;
	}
	return FOpenMobileDeviceMemoryInfo::Build(
		[[NSProcessInfo processInfo] physicalMemory],
		os_proc_available_memory(),
		true,
		PressureState
	);
#endif
}

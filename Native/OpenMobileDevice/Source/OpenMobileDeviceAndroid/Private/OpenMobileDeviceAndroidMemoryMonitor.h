#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"
#include "OpenMobileDeviceResourceTypes.h"

void ApplyOpenMobileDeviceAndroidMemoryPressureEvent(
	FOpenMobileMemorySnapshot& Snapshot
);
bool StartOpenMobileDeviceAndroidMemoryMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidMemoryMonitoring();

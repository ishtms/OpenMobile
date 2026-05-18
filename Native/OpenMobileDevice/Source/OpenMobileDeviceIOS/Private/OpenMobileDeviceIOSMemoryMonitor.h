#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"
#include "OpenMobileDeviceResourceTypes.h"

void ApplyOpenMobileDeviceIOSMemoryPressureEvent(
	FOpenMobileMemorySnapshot& Snapshot
);
bool StartOpenMobileDeviceIOSMemoryMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceIOSMemoryMonitoring();

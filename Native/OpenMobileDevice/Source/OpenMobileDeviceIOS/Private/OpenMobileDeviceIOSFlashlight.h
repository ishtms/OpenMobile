#pragma once

#include "OpenMobileDeviceFlashlightTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"

FOpenMobileFlashlightSnapshot GetOpenMobileDeviceIOSFlashlightSnapshot();
bool StartOpenMobileDeviceIOSFlashlightMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceIOSFlashlightMonitoring();

#pragma once

#include "OpenMobileDeviceFlashlightTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"

FOpenMobileFlashlightSnapshot GetOpenMobileDeviceAndroidFlashlightSnapshot();
bool StartOpenMobileDeviceAndroidFlashlightMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidFlashlightMonitoring();

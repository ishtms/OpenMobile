#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"
#include "OpenMobileDeviceResourceTypes.h"

FOpenMobilePowerSnapshot GetOpenMobileDeviceAndroidPowerSnapshot();
bool StartOpenMobileDeviceAndroidBatteryMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidBatteryMonitoring();

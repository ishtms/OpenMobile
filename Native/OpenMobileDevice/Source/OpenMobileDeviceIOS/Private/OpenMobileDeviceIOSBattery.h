#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"
#include "OpenMobileDeviceResourceTypes.h"

FOpenMobilePowerSnapshot GetOpenMobileDeviceIOSPowerSnapshot();
bool StartOpenMobileDeviceIOSBatteryMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceIOSBatteryMonitoring();

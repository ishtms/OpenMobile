#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"

bool StartOpenMobileDeviceAndroidNetworkMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidNetworkMonitoring();

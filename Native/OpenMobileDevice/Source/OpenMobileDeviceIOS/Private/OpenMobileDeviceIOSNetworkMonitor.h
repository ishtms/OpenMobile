#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"

bool StartOpenMobileDeviceIOSNetworkMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceIOSNetworkMonitoring();

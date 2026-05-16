#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"

bool StartOpenMobileDeviceAndroidLocaleMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidLocaleMonitoring();

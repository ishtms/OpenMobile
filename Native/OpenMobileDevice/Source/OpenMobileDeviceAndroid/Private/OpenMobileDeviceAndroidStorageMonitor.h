#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"

bool StartOpenMobileDeviceAndroidStorageMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidStorageMonitoring();

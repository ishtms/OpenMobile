#pragma once

#include "OpenMobileDeviceMonitoringCallback.h"

bool StartOpenMobileDeviceAndroidWindowMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidWindowMonitoring();

#pragma once

#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"

FOpenMobileAccessibilitySnapshot
GetOpenMobileDeviceAndroidAccessibilitySnapshot();
bool StartOpenMobileDeviceAndroidAccessibilityMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidAccessibilityMonitoring();

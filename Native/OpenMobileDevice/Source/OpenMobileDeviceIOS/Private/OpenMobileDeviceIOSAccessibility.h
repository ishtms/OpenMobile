#pragma once

#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"

FOpenMobileAccessibilitySnapshot GetOpenMobileDeviceIOSAccessibilitySnapshot();
bool StartOpenMobileDeviceIOSAccessibilityMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceIOSAccessibilityMonitoring();

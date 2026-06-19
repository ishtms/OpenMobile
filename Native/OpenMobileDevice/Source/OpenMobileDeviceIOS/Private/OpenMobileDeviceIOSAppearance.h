#pragma once

#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"

FOpenMobileAppearanceSnapshot GetOpenMobileDeviceIOSAppearanceSnapshot();
bool StartOpenMobileDeviceIOSAppearanceMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceIOSAppearanceMonitoring();

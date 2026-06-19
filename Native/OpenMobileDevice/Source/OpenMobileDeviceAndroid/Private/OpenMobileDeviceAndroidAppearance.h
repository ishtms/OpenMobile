#pragma once

#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"

FOpenMobileAppearanceSnapshot GetOpenMobileDeviceAndroidAppearanceSnapshot();
bool StartOpenMobileDeviceAndroidAppearanceMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidAppearanceMonitoring();

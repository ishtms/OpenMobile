#pragma once

#include "OpenMobileDeviceFlashlightTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"

FOpenMobileFlashlightSnapshot GetOpenMobileDeviceAndroidFlashlightSnapshot();
FOpenMobileFlashlightOperationResult ApplyOpenMobileDeviceAndroidFlashlight(
	const FOpenMobileFlashlightRequest& Request
);
void ClearOpenMobileDeviceAndroidFlashlight();
bool StartOpenMobileDeviceAndroidFlashlightMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceAndroidFlashlightMonitoring();

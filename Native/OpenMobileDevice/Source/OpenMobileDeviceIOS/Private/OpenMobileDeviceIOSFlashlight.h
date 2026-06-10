#pragma once

#include "OpenMobileDeviceFlashlightTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"

FOpenMobileFlashlightSnapshot GetOpenMobileDeviceIOSFlashlightSnapshot();
FOpenMobileFlashlightOperationResult ApplyOpenMobileDeviceIOSFlashlight(
	const FOpenMobileFlashlightRequest& Request
);
void ClearOpenMobileDeviceIOSFlashlight();
bool StartOpenMobileDeviceIOSFlashlightMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
);
void StopOpenMobileDeviceIOSFlashlightMonitoring();

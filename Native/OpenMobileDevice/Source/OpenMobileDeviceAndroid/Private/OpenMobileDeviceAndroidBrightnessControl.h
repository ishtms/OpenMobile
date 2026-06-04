#pragma once

#include "OpenMobileDeviceBrightnessControl.h"

FOpenMobileBrightnessSnapshot GetOpenMobileDeviceAndroidBrightnessSnapshot();

FOpenMobileBrightnessResult ApplyOpenMobileDeviceAndroidBrightness(
	const FOpenMobileBrightnessRequest& Request
);

void ClearOpenMobileDeviceAndroidBrightness();

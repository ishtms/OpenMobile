#pragma once

#include "OpenMobileDeviceBrightnessControl.h"

FOpenMobileBrightnessSnapshot GetOpenMobileDeviceIOSBrightnessSnapshot();

FOpenMobileBrightnessResult ApplyOpenMobileDeviceIOSBrightness(
	const FOpenMobileBrightnessRequest& Request
);

void ClearOpenMobileDeviceIOSBrightness();

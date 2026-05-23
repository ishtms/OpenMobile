#pragma once

#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceRefreshRateControl.h"

FOpenMobileWindowDisplaySnapshot
GetOpenMobileDeviceAndroidWindowDisplaySnapshot();

FOpenMobilePreferredRefreshRateResult
ApplyOpenMobileDeviceAndroidPreferredRefreshRate(
	const FOpenMobilePreferredRefreshRateRequest& Request
);

void ClearOpenMobileDeviceAndroidPreferredRefreshRate();

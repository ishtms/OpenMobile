#pragma once

#include "OpenMobileDeviceOrientationControl.h"

FOpenMobileOrientationPolicyResult ApplyOpenMobileDeviceIOSOrientationPolicy(
	const FOpenMobileOrientationPolicyRequest& Request
);

void ClearOpenMobileDeviceIOSOrientationPolicy();

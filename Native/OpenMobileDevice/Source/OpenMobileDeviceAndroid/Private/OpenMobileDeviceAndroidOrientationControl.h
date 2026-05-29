#pragma once

#include "OpenMobileDeviceOrientationControl.h"

FOpenMobileOrientationPolicyResult
ApplyOpenMobileDeviceAndroidOrientationPolicy(
	const FOpenMobileOrientationPolicyRequest& Request
);

void ClearOpenMobileDeviceAndroidOrientationPolicy();

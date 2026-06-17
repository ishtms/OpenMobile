#pragma once

#include "OpenMobileDeviceAndroidPackageTypes.h"

class FOpenMobileDeviceAndroidPackageCheckService final
{
public:
	static FOpenMobileAndroidPackageCheckResult Check(
		const FOpenMobileAndroidPackageCheckRequest& Request
	);
};

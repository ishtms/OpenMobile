#pragma once

#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceFlashlightTypes.h"

class FOpenMobileDeviceFlashlightControlPolicy final
{
public:
	static bool Validate(
		const FOpenMobileFlashlightRequest& Request,
		FOpenMobileError& OutError
	);
};

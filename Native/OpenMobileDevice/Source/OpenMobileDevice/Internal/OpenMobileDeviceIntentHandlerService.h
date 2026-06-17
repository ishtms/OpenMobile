#pragma once

#include "OpenMobileDeviceIntentHandlerTypes.h"

class FOpenMobileDeviceIntentHandlerService final
{
public:
	static FOpenMobileIntentHandlerCheckResult Check(
		const FOpenMobileIntentHandlerCheckRequest& Request
	);
};

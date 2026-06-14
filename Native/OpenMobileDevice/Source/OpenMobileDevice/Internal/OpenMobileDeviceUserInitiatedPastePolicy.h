#pragma once

#include "OpenMobileDeviceUserInitiatedPasteTypes.h"

class FOpenMobileDeviceUserInitiatedPastePolicy final
{
public:
	static bool Validate(
		const FOpenMobileUserInitiatedPasteRequest& Request,
		FOpenMobileError& OutError
	);
};

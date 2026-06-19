#pragma once

#include "OpenMobileDeviceDisplayTypes.h"

class FOpenMobileDeviceSystemAppearance final
{
public:
	static EOpenMobileSystemAppearance FromAndroidNightMode(
		int32 NightMode
	);
	static EOpenMobileSystemAppearance FromIOSUserInterfaceStyle(
		int32 UserInterfaceStyle
	);
};

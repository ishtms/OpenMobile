#include "OpenMobileDeviceSystemAppearance.h"

EOpenMobileSystemAppearance
FOpenMobileDeviceSystemAppearance::FromAndroidNightMode(int32 NightMode)
{
	if (NightMode == 0x10)
	{
		return EOpenMobileSystemAppearance::Light;
	}
	if (NightMode == 0x20)
	{
		return EOpenMobileSystemAppearance::Dark;
	}
	return EOpenMobileSystemAppearance::Unknown;
}

EOpenMobileSystemAppearance
FOpenMobileDeviceSystemAppearance::FromIOSUserInterfaceStyle(
	int32 UserInterfaceStyle
)
{
	if (UserInterfaceStyle == 1)
	{
		return EOpenMobileSystemAppearance::Light;
	}
	if (UserInterfaceStyle == 2)
	{
		return EOpenMobileSystemAppearance::Dark;
	}
	return EOpenMobileSystemAppearance::Unknown;
}

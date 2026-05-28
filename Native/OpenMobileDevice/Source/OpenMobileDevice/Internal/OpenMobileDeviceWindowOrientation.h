#pragma once

#include "OpenMobileDeviceDisplayTypes.h"

class FOpenMobileDeviceWindowOrientation final
{
public:
	static EOpenMobileWindowOrientation FromAndroidRotation(
		int32 Rotation,
		bool bNaturalOrientationLandscape
	);
	static EOpenMobileWindowOrientation FromIOSInterfaceOrientation(
		int32 InterfaceOrientation
	);
};

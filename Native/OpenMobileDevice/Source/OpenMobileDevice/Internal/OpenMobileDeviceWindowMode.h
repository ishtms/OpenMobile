#pragma once

#include "OpenMobileDeviceDisplayTypes.h"

class FOpenMobileDeviceWindowMode final
{
public:
	static EOpenMobileWindowMode Normalize(EOpenMobileWindowMode Mode);
	static EOpenMobileWindowMode FromAndroid(
		bool bIsInMultiWindowMode,
		bool bIsInPictureInPictureMode
	);
	static EOpenMobileWindowMode FromIOS(bool bMatchesScreenBounds);
};

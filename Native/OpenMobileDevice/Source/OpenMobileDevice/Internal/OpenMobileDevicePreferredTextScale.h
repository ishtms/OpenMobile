#pragma once

#include "OpenMobileDeviceAccessibilityTypes.h"

class FOpenMobileDevicePreferredTextScale final
{
public:
	static FOpenMobileAccessibilitySnapshot FromAndroidFontScale(
		float FontScale
	);
	static FOpenMobileAccessibilitySnapshot FromIOSContentSizeCategory(
		FString ContentSizeCategory,
		float RelativeScale
	);
};

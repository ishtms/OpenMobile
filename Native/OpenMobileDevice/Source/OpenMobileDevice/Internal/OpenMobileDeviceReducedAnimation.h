#pragma once

#include "OpenMobileDeviceAccessibilityTypes.h"

class FOpenMobileDeviceReducedAnimation final
{
public:
	static FOpenMobileAccessibilitySnapshot FromAndroidAnimationScales(
		float AnimatorScale,
		float TransitionScale,
		float WindowScale
	);
	static FOpenMobileAccessibilitySnapshot FromIOSReduceMotion(
		bool bEnabled
	);
};

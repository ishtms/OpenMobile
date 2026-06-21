#pragma once

#include "OpenMobileDeviceAccessibilityTypes.h"

class FOpenMobileDeviceAssistiveTechnology final
{
public:
	static FOpenMobileAccessibilitySnapshot
	FromAndroidTouchExplorationState(int32 State);
	static FOpenMobileAccessibilitySnapshot FromIOSVoiceOverState(
		bool bActive
	);
};

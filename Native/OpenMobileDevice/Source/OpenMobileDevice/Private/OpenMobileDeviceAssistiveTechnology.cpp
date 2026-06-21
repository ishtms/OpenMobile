#include "OpenMobileDeviceAssistiveTechnology.h"

FOpenMobileAccessibilitySnapshot
FOpenMobileDeviceAssistiveTechnology::FromAndroidTouchExplorationState(
	int32 State
)
{
	FOpenMobileAccessibilitySnapshot Snapshot;
	if (State == 0 || State == 1)
	{
		Snapshot.bTouchExplorationActive =
			FOpenMobileDeviceOptionalBool::MakeAvailable(State == 1);
	}
	return Snapshot;
}

FOpenMobileAccessibilitySnapshot
FOpenMobileDeviceAssistiveTechnology::FromIOSVoiceOverState(bool bActive)
{
	FOpenMobileAccessibilitySnapshot Snapshot;
	Snapshot.bScreenReaderActive =
		FOpenMobileDeviceOptionalBool::MakeAvailable(bActive);
	return Snapshot;
}

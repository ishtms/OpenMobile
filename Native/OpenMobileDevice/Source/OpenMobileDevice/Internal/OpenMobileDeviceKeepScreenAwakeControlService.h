#pragma once

#include "OpenMobileDeviceKeepScreenAwakeControl.h"

class FOpenMobileDeviceKeepScreenAwakeControlService final
{
public:
	static void Start();
	static void Shutdown();
	static FGuid AddRequest(FOpenMobileKeepScreenAwakeResult& OutResult);
	static void RemoveRequest(const FGuid& RequestId);

#if WITH_DEV_AUTOMATION_TESTS
	static void NotifySurfaceChangedForTests();
	static void NotifyBackgroundForTests();
	static void NotifyForegroundForTests();
	static void ResetForTests();
#endif
};

#pragma once

#include "OpenMobileDeviceSystemUiControl.h"

class FOpenMobileDeviceSystemUiControlService final
{
public:
	static void Start();
	static void Shutdown();
	static FGuid AddRequest(
		const FOpenMobileSystemUiRequest& Request,
		FOpenMobileSystemUiResult& OutResult
	);
	static void RemoveRequest(const FGuid& RequestId);

#if WITH_DEV_AUTOMATION_TESTS
	static void NotifySurfaceChangedForTests();
	static void NotifyBackgroundForTests();
	static void NotifyForegroundForTests();
	static void ResetForTests();
#endif
};

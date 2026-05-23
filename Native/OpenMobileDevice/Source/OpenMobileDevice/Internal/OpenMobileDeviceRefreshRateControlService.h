#pragma once

#include "OpenMobileDeviceRefreshRateControl.h"

class FOpenMobileDeviceRefreshRateControlService final
{
public:
	static void Start();
	static void Shutdown();
	static FGuid AddRequest(
		const FOpenMobilePreferredRefreshRateRequest& Request,
		FOpenMobilePreferredRefreshRateResult& OutResult
	);
	static void RemoveRequest(const FGuid& RequestId);

#if WITH_DEV_AUTOMATION_TESTS
	static void NotifySurfaceChangedForTests();
	static void NotifyBackgroundForTests();
	static void NotifyForegroundForTests();
	static void ResetForTests();
#endif
};

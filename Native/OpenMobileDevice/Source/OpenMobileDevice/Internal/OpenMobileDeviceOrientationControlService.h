#pragma once

#include "OpenMobileDeviceOrientationControl.h"

class FOpenMobileDeviceOrientationControlService final
{
public:
	static void Start();
	static void Shutdown();
	static FGuid AddRequest(
		const FOpenMobileOrientationPolicyRequest& Request,
		FOpenMobileOrientationPolicyResult& OutResult
	);
	static void RemoveRequest(const FGuid& RequestId);

#if WITH_DEV_AUTOMATION_TESTS
	static void NotifySurfaceChangedForTests();
	static void NotifyBackgroundForTests();
	static void NotifyForegroundForTests();
	static void ResetForTests();
#endif
};

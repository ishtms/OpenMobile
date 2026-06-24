#pragma once

#include "OpenMobileDeviceBrightnessControl.h"

class FOpenMobileDeviceBrightnessControlService final
{
public:
	static void Start();
	static void Shutdown();
	static FGuid AddRequest(
		const FOpenMobileBrightnessRequest& Request,
		FOpenMobileBrightnessResult& OutResult
	);
	static void RemoveRequest(const FGuid& RequestId);
	static int32 GetActiveRequestCountForDiagnostics();

#if WITH_DEV_AUTOMATION_TESTS
	static void NotifySurfaceChangedForTests();
	static void NotifyBackgroundForTests();
	static void NotifyForegroundForTests();
	static void ResetForTests();
#endif
};

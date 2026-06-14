#pragma once

#include "OpenMobileDeviceUserInitiatedPasteCallback.h"

class FOpenMobileDeviceUserInitiatedPasteService final
{
public:
	static void Start();
	static void Shutdown();
	static bool Begin(
		const FOpenMobileUserInitiatedPasteRequest& Request,
		FGuid& OutOperationId,
		FOpenMobileDeviceUserInitiatedPasteCompletion&& Completion,
		FOpenMobileError& OutError
	);
	static void Cancel(const FGuid& OperationId);

#if WITH_DEV_AUTOMATION_TESTS
	static void SetApplicationActiveForTests(bool bActive);
	static bool HasActiveOperationForTests();
#endif
};

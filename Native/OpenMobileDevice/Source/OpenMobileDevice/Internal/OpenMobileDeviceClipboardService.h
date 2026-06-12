#pragma once

#include "OpenMobileDeviceClipboardTypes.h"

class FOpenMobileDeviceClipboardService final
{
public:
	static void Start();
	static void Shutdown();
	static FOpenMobileClipboardOperationResult CheckContentTypes();
	static FOpenMobileClipboardOperationResult Write(
		const FOpenMobileClipboardWriteRequest& Request
	);
	static FOpenMobileClipboardOperationResult Read(
		EOpenMobileClipboardContentType ContentType
	);
	static FOpenMobileClipboardOperationResult Clear();

#if WITH_DEV_AUTOMATION_TESTS
	static void SetApplicationActiveForTests(bool bActive);
#endif
};

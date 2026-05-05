#pragma once

#include "CoreMinimal.h"

class IOpenMobileDeviceBackend;

struct FOpenMobileDeviceCallbackToken
{
	uint64 Generation = 0;
};

class OPENMOBILEDEVICE_API FOpenMobileDeviceBackendRegistry final
{
public:
	static void Start();
	static bool RegisterBackend(IOpenMobileDeviceBackend& Backend);
	static bool UnregisterBackend(IOpenMobileDeviceBackend& Backend);
	static IOpenMobileDeviceBackend* FindBackend();
	static FOpenMobileDeviceCallbackToken CaptureCallbackToken();
	static bool IsCallbackCurrent(const FOpenMobileDeviceCallbackToken& Token);
	static bool IsShuttingDown();
	static void BeginShutdown();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};

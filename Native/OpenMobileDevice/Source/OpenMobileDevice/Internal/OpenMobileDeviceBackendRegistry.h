#pragma once

#include "CoreMinimal.h"

class IOpenMobileDeviceBackend;
struct FOpenMobileStorageSnapshot;
struct FOpenMobileError;

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
	static bool IsBackendRegistered(const IOpenMobileDeviceBackend* Backend);
	static IOpenMobileDeviceBackend* FindBackend();
	static FOpenMobileDeviceCallbackToken CaptureCallbackToken();
	static bool IsCallbackCurrent(const FOpenMobileDeviceCallbackToken& Token);
	// Worker-safe. Unregistration waits for an active synchronous query to return.
	static bool QueryStorageSnapshot(IOpenMobileDeviceBackend* Backend,
		const FOpenMobileDeviceCallbackToken& Token,
		FOpenMobileStorageSnapshot& OutSnapshot, FOpenMobileError& OutError);
	static bool IsShuttingDown();
	static void BeginShutdown();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};

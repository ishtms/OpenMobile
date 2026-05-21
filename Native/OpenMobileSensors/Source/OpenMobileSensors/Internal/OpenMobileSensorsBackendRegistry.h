#pragma once

#include "CoreMinimal.h"

class IOpenMobileSensorsBackend;

struct FOpenMobileSensorsBackendToken
{
	uint64 Generation = 0;
};

class OPENMOBILESENSORS_API FOpenMobileSensorsBackendRegistry final
{
public:
	static void Start();
	static bool RegisterBackend(IOpenMobileSensorsBackend& Backend);
	static bool UnregisterBackend(IOpenMobileSensorsBackend& Backend);
	static bool IsBackendRegistered(const IOpenMobileSensorsBackend* Backend);
	static IOpenMobileSensorsBackend* FindBackend();
	static FOpenMobileSensorsBackendToken CaptureToken();
	static bool IsTokenCurrent(const FOpenMobileSensorsBackendToken& Token);
	static bool IsShuttingDown();
	static void BeginShutdown();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};

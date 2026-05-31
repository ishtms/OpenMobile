#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

class IOpenMobileHapticsBackend;
struct FOpenMobileHapticsBackendRequestToken;

class OPENMOBILEHAPTICS_API FOpenMobileHapticsBackendRegistry final
{
public:
	static void Start();
	static bool RegisterBackend(IOpenMobileHapticsBackend& Backend);
	static bool UnregisterBackend(IOpenMobileHapticsBackend& Backend);
	static bool IsBackendRegistered(const IOpenMobileHapticsBackend* Backend);
	static IOpenMobileHapticsBackend* FindBackend();
	static void RefreshCapabilities();
	static FOpenMobileHapticCapabilities GetCapabilitySnapshot();
	static FOpenMobileHapticsBackendRequestToken CreateRequestToken(
		IOpenMobileHapticsBackend& Backend,
		bool bCreatePlaybackHandle
	);
	static bool IsCallbackCurrent(
		const FOpenMobileHapticsBackendRequestToken& Token
	);
	static void SetApplicationActive(bool bActive);
	static bool IsApplicationActive();
	static void NotifyLifecycleChange();
	static bool IsShuttingDown();
	static void BeginShutdown();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};

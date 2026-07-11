#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsLifecyclePolicy.h"
#include "OpenMobileHapticsTypes.h"

class IOpenMobileHapticsBackend;
class FOpenMobileHapticsTimelineManager;
struct FOpenMobileHapticsBackendRequestToken;
enum class EOpenMobileHapticsInterruptionReason : uint8;

struct FOpenMobileHapticsInterruption
{
	FName BackendName;
	EOpenMobileHapticsInterruptionReason Reason;
	uint64 LifecycleGeneration = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticsInterruptionDelegate,
	const FOpenMobileHapticsInterruption&
);
DECLARE_MULTICAST_DELEGATE(FOpenMobileHapticsRecoveryDelegate);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticsApplicationLifecycleDelegate,
	const FOpenMobileHapticsLifecycleTransition&
);

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
	static EOpenMobileHapticsApplicationState GetApplicationState();
	static void NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent Event
	);
	static uint64 GetLifecycleGeneration();
	static FOpenMobileHapticsTimelineManager& GetTimelineManager();
	static void NotifyLifecycleChange();
	static void NotifyInterruption(
		FName BackendName,
		EOpenMobileHapticsInterruptionReason Reason
	);
	static bool RequestRecovery(bool bPolicyAllowsRecovery);
	static bool IsRecovering();
	static FOpenMobileHapticsInterruptionDelegate& OnInterruption();
	static FOpenMobileHapticsRecoveryDelegate& OnRecovery();
	static FOpenMobileHapticsApplicationLifecycleDelegate&
	OnApplicationLifecycle();
	static bool IsShuttingDown();
	static void BeginShutdown();

#if WITH_DEV_AUTOMATION_TESTS
	static void RunRecoveryAttemptForTests(double NowSeconds);
	static void ResetForTests();
#endif
};

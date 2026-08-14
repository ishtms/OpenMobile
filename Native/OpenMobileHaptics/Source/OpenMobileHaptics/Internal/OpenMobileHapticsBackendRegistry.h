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
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHapticsCapabilitiesChangedDelegate,
	const FOpenMobileHapticCapabilities&,
	const FOpenMobileHapticCapabilities&
);

class OPENMOBILEHAPTICS_API FOpenMobileHapticsBackendRegistry final
{
public:
	/** Connects registry lifecycle delegates once before any platform backend can be selected. */
	static void Start();
	/** Registers one backend and refreshes selection when it becomes the best available candidate. */
	static bool RegisterBackend(IOpenMobileHapticsBackend& Backend);
	/** Removes one exact backend instance and invalidates tokens if the active choice changes. */
	static bool UnregisterBackend(IOpenMobileHapticsBackend& Backend);
	/** Checks pointer identity under the registry lock, names aren't enough when modules reload. */
	static bool IsBackendRegistered(const IOpenMobileHapticsBackend* Backend);
	/** Returns the highest-priority available backend for the current generation. */
	static IOpenMobileHapticsBackend* FindBackend();
	/** Re-queries the selected backend and broadcasts only when the capability snapshot changed. */
	static void RefreshCapabilities();
	/** Returns the last stable capability copy without handing out backend state. */
	static FOpenMobileHapticCapabilities GetCapabilitySnapshot();
	/** Binds a request to backend and registry generation, optionally giving controllable playback a public handle. */
	static FOpenMobileHapticsBackendRequestToken CreateRequestToken(
		IOpenMobileHapticsBackend& Backend,
		bool bCreatePlaybackHandle
	);
	/** Rejects callbacks from replaced backends, old lifecycle generations, or malformed tokens. */
	static bool IsCallbackCurrent(
		const FOpenMobileHapticsBackendRequestToken& Token
	);
	/** Keeps the compatibility active flag routed through the richer lifecycle state. */
	static void SetApplicationActive(bool bActive);
	/** Reports active only from the registry's accepted application state. */
	static bool IsApplicationActive();
	/** Gives request policies the latest lifecycle state without subscribing to delegates. */
	static EOpenMobileHapticsApplicationState GetApplicationState();
	/** Applies an engine lifecycle event, invalidates stale starts, and forwards the resolved transition. */
	static void NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent Event
	);
	/** Changes whenever scheduled work and callbacks must stop belonging to the old native lifecycle. */
	static uint64 GetLifecycleGeneration();
	/** Owns the one prepared timeline cache shared by the selected backend generation. */
	static FOpenMobileHapticsTimelineManager& GetTimelineManager();
	/** Refreshes native services for callers that don't have a more specific lifecycle event. */
	static void NotifyLifecycleChange();
	/** Starts interruption recovery only for the active backend and current generation. */
	static void NotifyInterruption(
		FName BackendName,
		EOpenMobileHapticsInterruptionReason Reason
	);
	/** Attempts bounded recovery when application and policy allow it, permanent failure ends the window. */
	static bool RequestRecovery(bool bPolicyAllowsRecovery);
	/** Tells the subsystem whether the registry is still inside an interruption recovery window. */
	static bool IsRecovering();
	/** Exposes interruption notification without allowing callers to replace registry ownership. */
	static FOpenMobileHapticsInterruptionDelegate& OnInterruption();
	/** Exposes successful recovery so the subsystem can refresh resources and pending state. */
	static FOpenMobileHapticsRecoveryDelegate& OnRecovery();
	/** Exposes resolved application transitions after generation and backend state are updated. */
	static FOpenMobileHapticsApplicationLifecycleDelegate&
	OnApplicationLifecycle();
	/** Exposes capability changes as old and new snapshots for caller-side decisions. */
	static FOpenMobileHapticsCapabilitiesChangedDelegate&
	OnCapabilitiesChanged();
	/** Prevents submissions and registration churn once process teardown begins. */
	static bool IsShuttingDown();
	/** Seals the registry, invalidates generations, and asks every registered backend to disconnect. */
	static void BeginShutdown();

#if WITH_DEV_AUTOMATION_TESTS
	/** Drives one recovery tick with a supplied clock so backoff behavior stays deterministic in tests. */
	static void RunRecoveryAttemptForTests(double NowSeconds);
	/** Clears static registry state between tests, including delegates and selected backend. */
	static void ResetForTests();
#endif
};

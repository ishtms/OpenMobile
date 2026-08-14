#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileHapticsBackend.h"
#include "OpenMobileHapticsAppleContinuousPolicy.h"
#include "OpenMobileHapticsAppleTransientPolicy.h"
#include "OpenMobileHapticsSemanticPolicy.h"

enum class EOpenMobileHapticsAppleHardwareState : uint8
{
	Supported,
	Unsupported,
	TemporarilyUnavailable
};

struct FOpenMobileHapticsAppleHardwareProbe
{
	EOpenMobileHapticsAppleHardwareState RichHaptics =
		EOpenMobileHapticsAppleHardwareState::TemporarilyUnavailable;
	bool bSupportsAudio = false;
	int32 OSMajorVersion = 0;
};

enum class EOpenMobileHapticsAppleEngineResult : uint8
{
	Ready,
	UnsupportedHardware,
	TemporarilyUnavailable,
	NativeFailure,
	ShuttingDown
};

enum class EOpenMobileHapticsAppleSubmissionResult : uint8
{
	Accepted,
	Unsupported,
	StaleRequest,
	NativeFailure,
	ShuttingDown
};

enum class EOpenMobileHapticsAppleBridgeEvent : uint8
{
	EngineStopped,
	EngineReset,
	AudioSessionChanged
};

using FOpenMobileHapticsAppleBridgeEventCallback =
	TFunction<void(EOpenMobileHapticsAppleBridgeEvent)>;

enum class EOpenMobileHapticsApplePlaybackEvent : uint8
{
	Completed,
	Interrupted,
	Failed
};

using FOpenMobileHapticsApplePlaybackEventCallback =
	TFunction<void(EOpenMobileHapticsApplePlaybackEvent)>;

struct FOpenMobileHapticsApplePlaybackSchedule
{
	double PlatformTimeSeconds = 0.0;
	double MaximumLatenessSeconds = 0.05;
	TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> Guard;

	/** Requires a future-capable platform time, a bounded lateness window, and a live lifecycle guard before scheduling. */
	bool IsValid() const
	{
		return FMath::IsFinite(PlatformTimeSeconds)
			&& PlatformTimeSeconds > 0.0
			&& FMath::IsFinite(MaximumLatenessSeconds)
			&& MaximumLatenessSeconds >= 0.0
			&& MaximumLatenessSeconds <= 1.0
			&& Guard.IsValid();
	}
};

struct FOpenMobileHapticsAppleAudioResource
{
	FString RelativePath;
	TArray<uint8> Data;
};

struct FOpenMobileHapticsAppleAHAPPattern
{
	FString NormalizedJson;
	TArray<FOpenMobileHapticsAppleAudioResource> AudioResources;
	double DurationSeconds = 0.0;
	double SafetyDurationSeconds = 0.0;
	double ScheduledPlatformTimeSeconds = 0.0;
	double MaximumLatenessSeconds = 0.05;
	bool bRequiresAdvancedPlayer = false;
	bool bLoop = false;
	bool bScheduled = false;
	bool bHasInitialDynamicParameters = false;
	FOpenMobileHapticDynamicParameterUpdate InitialDynamicParameters;
};

class OPENMOBILEHAPTICS_API IOpenMobileHapticsAppleBridge
{
public:
	/** Lets the Objective-C bridge release its engine, players, and callback blocks through the implementation. */
	virtual ~IOpenMobileHapticsAppleBridge() = default;

	/** Queries hardware and OS support without creating a Core Haptics engine. */
	virtual FOpenMobileHapticsAppleHardwareProbe QueryHardware() = 0;
	/** Creates and starts the native engine once hardware has been accepted. */
	virtual EOpenMobileHapticsAppleEngineResult CreateEngine() = 0;
	/** Warms semantic generators and lets them expire after the supplied idle lifetime. */
	virtual EOpenMobileHapticsAppleSubmissionResult PrepareSemanticGenerators(
		double IdleLifetimeSeconds
	) = 0;
	/** Caches one transient player by resource id within the shared prepared-resource budget. */
	virtual EOpenMobileHapticsAppleSubmissionResult PrepareTransientPattern(
		uint64 ResourceId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	) = 0;
	/** Caches one continuous player only after its estimated bytes fit the native preparation limits. */
	virtual EOpenMobileHapticsAppleSubmissionResult PrepareContinuousPattern(
		uint64 ResourceId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	) = 0;
	/** Plays an immediate system semantic after intensity has already been resolved by policy. */
	virtual EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity
	) = 0;
	/** Uses Apple's ordinary system vibration when richer semantic feedback isn't the selected route. */
	virtual EOpenMobileHapticsAppleSubmissionResult
	PlaySystemVibration() = 0;
	/** Schedules semantic playback against platform time and reports terminal state through one callback. */
	virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledSemantic(
		uint64 RequestId,
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	/** Schedules the system vibration route with the same lifecycle and lateness checks as richer playback. */
	virtual EOpenMobileHapticsAppleSubmissionResult
	PlayScheduledSystemVibration(
		uint64 RequestId,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	/** Starts a transient pattern immediately, reusing a prepared player when the supplied resource id is still valid. */
	virtual EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	) = 0;
	/** Starts a continuous pattern and binds live parameter updates to its request id. */
	virtual EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	) = 0;
	/** Schedules transient playback without letting a late lifecycle generation start native work. */
	virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	) = 0;
	/** Schedules continuous playback and preserves its request id for later controls. */
	virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	) = 0;
	/** Plays normalized AHAP and registers any validated audio resources before creating its player. */
	virtual EOpenMobileHapticsAppleSubmissionResult PlayAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	/** Schedules normalized AHAP using the same guard and lateness rules as portable patterns. */
	virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	/** Stops the player owned by one request id and ignores unrelated native players. */
	virtual EOpenMobileHapticsAppleSubmissionResult StopPattern(
		uint64 RequestId
	) = 0;
	/** Pauses only players whose native type supports it, returning unsupported otherwise. */
	virtual EOpenMobileHapticsAppleSubmissionResult PausePattern(
		uint64 RequestId
	) = 0;
	/** Resumes an already paused native player without rebuilding its pattern. */
	virtual EOpenMobileHapticsAppleSubmissionResult ResumePattern(
		uint64 RequestId
	) = 0;
	/** Moves a controllable native player to an already validated position in seconds. */
	virtual EOpenMobileHapticsAppleSubmissionResult SeekPattern(
		uint64 RequestId,
		double PositionSeconds
	) = 0;
	/** Sends dynamic intensity or sharpness to the player currently owned by the request id. */
	virtual EOpenMobileHapticsAppleSubmissionResult UpdatePattern(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) = 0;
	/** Replaces the engine-event callback so service teardown can disconnect native notifications cleanly. */
	virtual void SetEventCallback(
		FOpenMobileHapticsAppleBridgeEventCallback Callback
	) = 0;
	/** Drops cached players and audio resources while leaving the engine available for immediate work. */
	virtual void ReleasePreparedResources() = 0;
	/** Stops engine activity and seals every callback during module teardown. */
	virtual void Shutdown() = 0;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsAppleBridgeService final
{
public:
	/** Takes sole ownership of the platform bridge and wraps its callbacks with service lifetime checks. */
	explicit FOpenMobileHapticsAppleBridgeService(
		TUniquePtr<IOpenMobileHapticsAppleBridge> InBridge
	);
	/** Disconnects callbacks before releasing the bridge, late native events mustn't reach freed state. */
	~FOpenMobileHapticsAppleBridgeService();

	/** Returns the cached stable probe when possible and re-queries temporary native states. */
	FOpenMobileHapticsAppleHardwareProbe GetHardwareProbe();
	/** Forces the next request to re-query hardware after an audio-session or lifecycle change. */
	void InvalidateHardwareProbe();
	/** Marks engine state stale without destroying the bridge, recovery can recreate it on demand. */
	void InvalidateEngine();
	/** Creates the engine once and translates shutdown or hardware state before any submission proceeds. */
	EOpenMobileHapticsAppleEngineResult EnsureEngine();
	/** Ensures engine availability and forwards semantic warming under the service lock. */
	EOpenMobileHapticsAppleSubmissionResult PrepareSemanticGenerators(
		double IdleLifetimeSeconds
	);
	/** Ensures the engine and forwards one transient preparation with its accounting data intact. */
	EOpenMobileHapticsAppleSubmissionResult PrepareTransientPattern(
		uint64 ResourceId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	);
	/** Ensures the engine before continuous player preparation, shutdown always wins over new cache work. */
	EOpenMobileHapticsAppleSubmissionResult PrepareContinuousPattern(
		uint64 ResourceId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	);
	/** Plays an immediate semantic only after service and engine state are usable. */
	EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity
	);
	/** Routes basic system vibration through the same engine lifetime checks as other Apple work. */
	EOpenMobileHapticsAppleSubmissionResult PlaySystemVibration();
	/** Validates service lifetime before forwarding scheduled semantic callback ownership. */
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledSemantic(
		uint64 RequestId,
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	/** Forwards scheduled system vibration while guarding callback delivery after shutdown. */
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledSystemVibration(
		uint64 RequestId,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	/** Relays transient completion through service-owned callback state so destruction stays safe. */
	EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	);
	/** Relays continuous completion and preserves request ownership for later player controls. */
	EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	);
	/** Forwards a scheduled transient only after the engine and schedule are both accepted. */
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	);
	/** Forwards scheduled continuous playback with guarded terminal callback delivery. */
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	);
	/** Sends normalized AHAP through the bridge and relays its terminal callback safely. */
	EOpenMobileHapticsAppleSubmissionResult PlayAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	/** Sends scheduled AHAP while preserving the caller's platform-time guard. */
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	/** Forwards stop only while the service is live, shutdown already owns all remaining players. */
	EOpenMobileHapticsAppleSubmissionResult StopPattern(uint64 RequestId);
	/** Forwards pause to the current native player without changing service engine state. */
	EOpenMobileHapticsAppleSubmissionResult PausePattern(uint64 RequestId);
	/** Forwards resume for an existing player, it won't recreate missing playback. */
	EOpenMobileHapticsAppleSubmissionResult ResumePattern(uint64 RequestId);
	/** Forwards a policy-validated seek position to the owned native player. */
	EOpenMobileHapticsAppleSubmissionResult SeekPattern(
		uint64 RequestId,
		double PositionSeconds
	);
	/** Forwards live parameters only while the request still owns an active player. */
	EOpenMobileHapticsAppleSubmissionResult UpdatePattern(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	);
	/** Installs the high-level engine callback through shared state that can be sealed at destruction. */
	void SetEventCallback(
		FOpenMobileHapticsAppleBridgeEventCallback Callback
	);
	/** Releases cached native resources without discarding stable hardware knowledge. */
	void ReleasePreparedResources();
	/** Seals callback state first, then shuts the platform bridge down exactly once. */
	void Shutdown();

private:
	struct FCallbackState;

	/** Reads or refreshes hardware state while the caller already owns the service mutex. */
	FOpenMobileHapticsAppleHardwareProbe GetHardwareProbeLocked();
	/** Wraps a playback callback with shared lifetime state so native completion after destruction becomes harmless. */
	FOpenMobileHapticsApplePlaybackEventCallback RelayPlaybackCallback(
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);

	TUniquePtr<IOpenMobileHapticsAppleBridge> Bridge;
	TSharedRef<FCallbackState, ESPMode::ThreadSafe> CallbackState;
	FCriticalSection Mutex;
	TOptional<FOpenMobileHapticsAppleHardwareProbe> StableProbe;
	bool bEngineReady = false;
	bool bShuttingDown = false;
};

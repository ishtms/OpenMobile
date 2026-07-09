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
	virtual ~IOpenMobileHapticsAppleBridge() = default;

	virtual FOpenMobileHapticsAppleHardwareProbe QueryHardware() = 0;
	virtual EOpenMobileHapticsAppleEngineResult CreateEngine() = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PrepareSemanticGenerators(
		double IdleLifetimeSeconds
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PrepareTransientPattern(
		uint64 ResourceId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PrepareContinuousPattern(
		uint64 ResourceId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult
	PlaySystemVibration() = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledSemantic(
		uint64 RequestId,
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult
	PlayScheduledSystemVibration(
		uint64 RequestId,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult StopPattern(
		uint64 RequestId
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PausePattern(
		uint64 RequestId
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult ResumePattern(
		uint64 RequestId
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult SeekPattern(
		uint64 RequestId,
		double PositionSeconds
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult UpdatePattern(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) = 0;
	virtual void SetEventCallback(
		FOpenMobileHapticsAppleBridgeEventCallback Callback
	) = 0;
	virtual void ReleasePreparedResources() = 0;
	virtual void Shutdown() = 0;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsAppleBridgeService final
{
public:
	explicit FOpenMobileHapticsAppleBridgeService(
		TUniquePtr<IOpenMobileHapticsAppleBridge> InBridge
	);
	~FOpenMobileHapticsAppleBridgeService();

	FOpenMobileHapticsAppleHardwareProbe GetHardwareProbe();
	void InvalidateHardwareProbe();
	void InvalidateEngine();
	EOpenMobileHapticsAppleEngineResult EnsureEngine();
	EOpenMobileHapticsAppleSubmissionResult PrepareSemanticGenerators(
		double IdleLifetimeSeconds
	);
	EOpenMobileHapticsAppleSubmissionResult PrepareTransientPattern(
		uint64 ResourceId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	);
	EOpenMobileHapticsAppleSubmissionResult PrepareContinuousPattern(
		uint64 ResourceId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	);
	EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity
	);
	EOpenMobileHapticsAppleSubmissionResult PlaySystemVibration();
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledSemantic(
		uint64 RequestId,
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledSystemVibration(
		uint64 RequestId,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	);
	EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	);
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	);
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr,
		uint64 PreparedResourceId = 0
	);
	EOpenMobileHapticsAppleSubmissionResult PlayAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	EOpenMobileHapticsAppleSubmissionResult PlayScheduledAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	EOpenMobileHapticsAppleSubmissionResult StopPattern(uint64 RequestId);
	EOpenMobileHapticsAppleSubmissionResult PausePattern(uint64 RequestId);
	EOpenMobileHapticsAppleSubmissionResult ResumePattern(uint64 RequestId);
	EOpenMobileHapticsAppleSubmissionResult SeekPattern(
		uint64 RequestId,
		double PositionSeconds
	);
	EOpenMobileHapticsAppleSubmissionResult UpdatePattern(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	);
	void SetEventCallback(
		FOpenMobileHapticsAppleBridgeEventCallback Callback
	);
	void ReleasePreparedResources();
	void Shutdown();

private:
	struct FCallbackState;

	FOpenMobileHapticsAppleHardwareProbe GetHardwareProbeLocked();
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

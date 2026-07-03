#pragma once

#include "CoreMinimal.h"
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
	EngineReset
};

using FOpenMobileHapticsAppleBridgeEventCallback =
	TFunction<void(EOpenMobileHapticsAppleBridgeEvent)>;

enum class EOpenMobileHapticsApplePlaybackEvent : uint8
{
	Completed,
	Failed
};

using FOpenMobileHapticsApplePlaybackEventCallback =
	TFunction<void(EOpenMobileHapticsApplePlaybackEvent)>;

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
	virtual EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult
	PlaySystemVibration() = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult PlayAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult StopPattern(
		uint64 RequestId
	) = 0;
	virtual EOpenMobileHapticsAppleSubmissionResult UpdatePattern(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) = 0;
	virtual void SetEventCallback(
		FOpenMobileHapticsAppleBridgeEventCallback Callback
	) = 0;
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
	EOpenMobileHapticsAppleEngineResult EnsureEngine();
	EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity
	);
	EOpenMobileHapticsAppleSubmissionResult PlaySystemVibration();
	EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleTransientPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr
	);
	EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleContinuousPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback,
		const FOpenMobileHapticDynamicParameterUpdate* InitialParameters =
			nullptr
	);
	EOpenMobileHapticsAppleSubmissionResult PlayAHAPPattern(
		uint64 RequestId,
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		FOpenMobileHapticsApplePlaybackEventCallback Callback
	);
	EOpenMobileHapticsAppleSubmissionResult StopPattern(uint64 RequestId);
	EOpenMobileHapticsAppleSubmissionResult UpdatePattern(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	);
	void SetEventCallback(
		FOpenMobileHapticsAppleBridgeEventCallback Callback
	);
	void Shutdown();

private:
	struct FCallbackState;

	FOpenMobileHapticsAppleHardwareProbe GetHardwareProbeLocked();

	TUniquePtr<IOpenMobileHapticsAppleBridge> Bridge;
	TSharedRef<FCallbackState, ESPMode::ThreadSafe> CallbackState;
	FCriticalSection Mutex;
	TOptional<FOpenMobileHapticsAppleHardwareProbe> StableProbe;
	bool bEngineReady = false;
	bool bShuttingDown = false;
};

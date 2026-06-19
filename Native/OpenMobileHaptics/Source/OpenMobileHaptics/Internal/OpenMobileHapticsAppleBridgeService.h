#pragma once

#include "CoreMinimal.h"
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

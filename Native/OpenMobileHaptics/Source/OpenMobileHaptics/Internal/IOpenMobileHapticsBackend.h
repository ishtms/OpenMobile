#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileHapticsOneShotPolicy.h"
#include "OpenMobileHapticsLifecyclePolicy.h"
#include "OpenMobileHapticsRepeatPolicy.h"
#include "OpenMobileHapticsSemanticPolicy.h"
#include "OpenMobileHapticsTimingPolicy.h"
#include "OpenMobileHapticsTypes.h"

struct FOpenMobileHapticsPortableTimeline;

enum class EOpenMobileHapticsInterruptionReason : uint8
{
	EngineStopped,
	EngineReset,
	AudioSessionChanged,
	ActivityReplaced,
	NativeServiceLost,
	BackendReplaced
};

enum class EOpenMobileHapticsRecoveryResult : uint8
{
	Recovered,
	RetryableFailure,
	PermanentFailure
};

struct FOpenMobileHapticsBackendPreparationRequest
{
	TArray<TSharedPtr<
		const FOpenMobileHapticsPortableTimeline,
		ESPMode::ThreadSafe
	>> Patterns;
	FOpenMobileHapticsPreparedResourceLimits Limits;
};

struct FOpenMobileHapticsBackendPreparationResult
{
	EOpenMobileHapticPreparationState State =
		EOpenMobileHapticPreparationState::Failed;
	TArray<FString> Errors;
};

struct FOpenMobileHapticsBackendControlSupport
{
	bool bStop = false;
	bool bStopChannel = false;
	bool bStopAll = false;
	bool bPause = false;
	bool bResume = false;
	bool bSeek = false;
	bool bDynamicParameters = false;
};

struct FOpenMobileHapticsBackendPlaybackControlSupport
{
	EOpenMobileHapticControlImplementation PauseImplementation =
		EOpenMobileHapticControlImplementation::Unsupported;
	EOpenMobileHapticControlImplementation ResumeImplementation =
		EOpenMobileHapticControlImplementation::Unsupported;
	EOpenMobileHapticControlImplementation SeekImplementation =
		EOpenMobileHapticControlImplementation::Unsupported;
	double SeekGranularitySeconds = 0.0;
	FOpenMobileHapticsRepeatPlan RepeatPlan;
	bool bHasRepeatPlan = false;

	bool SupportsAnyControl() const
	{
		return PauseImplementation
				== EOpenMobileHapticControlImplementation::Native
			|| PauseImplementation
				== EOpenMobileHapticControlImplementation::Emulated
			|| ResumeImplementation
				== EOpenMobileHapticControlImplementation::Native
			|| ResumeImplementation
				== EOpenMobileHapticControlImplementation::Emulated
			|| SeekImplementation
				== EOpenMobileHapticControlImplementation::Native
			|| SeekImplementation
				== EOpenMobileHapticControlImplementation::Emulated;
	}
};

struct FOpenMobileHapticsBackendControlCommand
{
	uint64 Revision = 0;
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;
	double RequestedPositionSeconds = 0.0;
	double ResolvedPositionSeconds = 0.0;
	double ActiveDurationSeconds = 0.0;
	double PositionGranularitySeconds = 0.0;
	int32 CompletedRepeatCount = 0;
	bool bQuantized = false;
};

struct FOpenMobileHapticsBackendRequestToken
{
	uint64 RegistryGeneration = 0;
	uint64 RequestId = 0;
	FName BackendName;
	FOpenMobileHapticPlaybackHandle PlaybackHandle;

	bool IsValid() const
	{
		return RegistryGeneration != 0
			&& RequestId != 0
			&& !BackendName.IsNone();
	}

	bool operator==(const FOpenMobileHapticsBackendRequestToken& Other) const
	{
		return RegistryGeneration == Other.RegistryGeneration
			&& RequestId == Other.RequestId
			&& BackendName == Other.BackendName
			&& PlaybackHandle == Other.PlaybackHandle;
	}
};

struct FOpenMobileHapticsBackendCallback
{
	FOpenMobileHapticsBackendRequestToken Token;
	uint64 Sequence = 0;
	FOpenMobileHapticPlaybackEvent Event;
};

using FOpenMobileHapticsBackendEventCallback =
	TFunction<void(const FOpenMobileHapticsBackendCallback&)>;

struct FOpenMobileHapticsBackendSubmission
{
	FOpenMobileHapticPlaybackResult Result;
	FOpenMobileHapticsBackendPlaybackControlSupport PlaybackControlSupport;
	bool bCreatesControllablePlayback = false;
	bool bExpectsCallbacks = false;
};

class FOpenMobileHapticsScheduledStartGuard final
{
public:
	explicit FOpenMobileHapticsScheduledStartGuard(
		uint64 InLifecycleGeneration
	)
		: LifecycleGeneration(InLifecycleGeneration)
	{
	}

	bool CanStart(uint64 CurrentLifecycleGeneration) const
	{
		return bValid.Load()
			&& LifecycleGeneration != 0
			&& LifecycleGeneration == CurrentLifecycleGeneration;
	}

	void Invalidate()
	{
		bValid.Store(false);
	}

private:
	TAtomic<bool> bValid{true};
	uint64 LifecycleGeneration = 0;
};

struct FOpenMobileHapticsBackendPlaybackParameters
{
	bool bHasInitialDynamicParameters = false;
	FOpenMobileHapticDynamicParameterUpdate InitialDynamicParameters;
	FOpenMobileHapticsTimingResolution Timing;
	TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> ScheduledStartGuard;
	TSharedPtr<
		const FOpenMobileHapticsPortableTimeline,
		ESPMode::ThreadSafe
	> PortableTimeline;
};

class IOpenMobileHapticsBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileHapticsBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Haptics.Backend"));
		return FeatureName;
	}

	virtual FName GetBackendName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsAvailable() const { return true; }
	virtual FOpenMobileHapticCapabilities GetCapabilities() const = 0;
	virtual bool IsCustomPlaybackConfigured() const { return true; }
	virtual EOpenMobileHapticPreparationState
	GetPreparationState() const = 0;
	virtual FOpenMobileHapticsBackendPreparationResult PrepareResources(
		const FOpenMobileHapticsBackendPreparationRequest& Request
	) = 0;
	virtual void ReleasePreparedResources() = 0;
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const = 0;
	virtual void HandleLifecycleChange() {}
	virtual void HandleApplicationLifecycle(
		const FOpenMobileHapticsLifecycleTransition& Transition
	)
	{
		static_cast<void>(Transition);
		HandleLifecycleChange();
	}
	virtual void HandleInterruption(
		EOpenMobileHapticsInterruptionReason Reason
	)
	{
		static_cast<void>(Reason);
		HandleLifecycleChange();
	}
	virtual EOpenMobileHapticsRecoveryResult RecoverFromInterruption()
	{
		return EOpenMobileHapticsRecoveryResult::Recovered;
	}

	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsOneShotResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;

	virtual FOpenMobileHapticControlResult StopPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token
	) = 0;
	virtual FOpenMobileHapticControlResult UpdatePlaybackParameters(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	)
	{
		static_cast<void>(Token);
		static_cast<void>(Update);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		return Result;
	}
	virtual FOpenMobileHapticControlResult PausePlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	)
	{
		static_cast<void>(Token);
		static_cast<void>(Command);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		Result.Implementation =
			EOpenMobileHapticControlImplementation::Unsupported;
		return Result;
	}
	virtual FOpenMobileHapticControlResult ResumePlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	)
	{
		static_cast<void>(Token);
		static_cast<void>(Command);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		Result.Implementation =
			EOpenMobileHapticControlImplementation::Unsupported;
		return Result;
	}
	virtual FOpenMobileHapticControlResult SeekPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	)
	{
		static_cast<void>(Token);
		static_cast<void>(Command);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		Result.Implementation =
			EOpenMobileHapticControlImplementation::Unsupported;
		return Result;
	}
	virtual FOpenMobileHapticControlResult StopChannel(FName Channel)
	{
		static_cast<void>(Channel);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		return Result;
	}
	virtual FOpenMobileHapticControlResult StopAll()
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		return Result;
	}
	virtual void BeginShutdown() = 0;
};

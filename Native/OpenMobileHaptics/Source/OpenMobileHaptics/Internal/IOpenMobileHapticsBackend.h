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

	/** Reports control only when at least one operation is native or can be emulated safely. */
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

	/** Requires registry generation, request id, and backend identity, the handle can stay empty for fire-and-forget work. */
	bool IsValid() const
	{
		return RegistryGeneration != 0
			&& RequestId != 0
			&& !BackendName.IsNone();
	}

	/** Compares ownership fields together so callbacks can't cross backend generations or playback handles. */
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
	uint64 PreviousSequence = MAX_uint64;
	FOpenMobileHapticPlaybackEvent Event;

	/** Separates callbacks with declared ordering from the first event in a stream. */
	bool HasExplicitPredecessor() const
	{
		return PreviousSequence != MAX_uint64;
	}
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
	/** Captures the lifecycle generation at scheduling time, so delayed starts can't survive an app or backend reset. */
	explicit FOpenMobileHapticsScheduledStartGuard(
		uint64 InLifecycleGeneration
	)
		: LifecycleGeneration(InLifecycleGeneration)
	{
	}

	/** Allows start only while the guard is live and its captured lifecycle still matches. */
	bool CanStart(uint64 CurrentLifecycleGeneration) const
	{
		return bValid.Load()
			&& LifecycleGeneration != 0
			&& LifecycleGeneration == CurrentLifecycleGeneration;
	}

	/** Cancels delayed work atomically, it may be called from teardown while the scheduler is on another thread. */
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
	/** Lets platform implementations release native state through their own destructor. */
	virtual ~IOpenMobileHapticsBackend() = default;

	/** Uses one stable modular-feature key so runtime modules can register without a direct dependency on the subsystem. */
	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Haptics.Backend"));
		return FeatureName;
	}

	/** Identifies the backend in request tokens, diagnostics, and replacement checks. */
	virtual FName GetBackendName() const = 0;
	/** Breaks ties when more than one available backend is registered for the platform. */
	virtual int32 GetPriority() const { return 0; }
	/** Lets a registered backend opt out temporarily when its native service isn't usable. */
	virtual bool IsAvailable() const { return true; }
	/** Returns a snapshot callers can retain without touching native objects. */
	virtual FOpenMobileHapticCapabilities GetCapabilities() const = 0;
	/** Reports whether project and platform setup allow custom playback, separate from hardware support. */
	virtual bool IsCustomPlaybackConfigured() const { return true; }
	/** Exposes native preparation state so named requests don't guess from cache contents. */
	virtual EOpenMobileHapticPreparationState
	GetPreparationState() const = 0;
	/** Prepares compiled resources as one backend-owned batch and returns validation errors without partial success. */
	virtual FOpenMobileHapticsBackendPreparationResult PrepareResources(
		const FOpenMobileHapticsBackendPreparationRequest& Request
	) = 0;
	/** Releases backend caches when libraries change or lifecycle invalidates their native objects. */
	virtual void ReleasePreparedResources() = 0;
	/** Advertises backend-wide control support before a request creates per-playback support. */
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const = 0;
	/** Preserves the older refresh hook for backends that don't need the full application transition. */
	virtual void HandleLifecycleChange() {}
	/** Receives the exact application transition while keeping older backends working through the refresh hook. */
	virtual void HandleApplicationLifecycle(
		const FOpenMobileHapticsLifecycleTransition& Transition
	)
	{
		static_cast<void>(Transition);
		HandleLifecycleChange();
	}
	/** Receives native interruption reason and defaults to the same state refresh used by lifecycle changes. */
	virtual void HandleInterruption(
		EOpenMobileHapticsInterruptionReason Reason
	)
	{
		static_cast<void>(Reason);
		HandleLifecycleChange();
	}
	/** Gives backends a retry point after interruption, stateless implementations can accept immediately. */
	virtual EOpenMobileHapticsRecoveryResult RecoverFromInterruption()
	{
		return EOpenMobileHapticsRecoveryResult::Recovered;
	}

	/** Submits a resolved semantic request with a token that must accompany every later callback. */
	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;
	/** Submits a resolved one-shot route, including scheduled timing and initial parameters when supported. */
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsOneShotResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;
	/** Submits prepared named playback after the subsystem has resolved library and timeline state. */
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;

	/** Stops the exact backend token, reused public handles can't cancel a newer registry generation. */
	virtual FOpenMobileHapticControlResult StopPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token
	) = 0;
	/** Updates live parameters when supported, the default returns unsupported without disturbing playback. */
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
	/** Pauses through a revisioned command, the default keeps unsupported behavior predictable. */
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
	/** Resumes through the same revisioned state contract used by emulated control. */
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
	/** Seeks to the already resolved position, leaving quantization decisions outside native code. */
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
	/** Stops a channel when the backend can do it directly, otherwise the subsystem stops individual tokens. */
	virtual FOpenMobileHapticControlResult StopChannel(FName Channel)
	{
		static_cast<void>(Channel);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		return Result;
	}
	/** Stops all native playback when supported, with per-token fallback handled by the subsystem. */
	virtual FOpenMobileHapticControlResult StopAll()
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		return Result;
	}
	/** Prevents new native work and disconnects callbacks before the module unregisters. */
	virtual void BeginShutdown() = 0;
};

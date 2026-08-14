#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsAsyncAction.generated.h"

class UOpenMobileHapticsSubsystem;
class UWorld;

UENUM()
enum class EOpenMobileHapticAsyncTerminalState : uint8
{
	Pending,
	Completed,
	Stopped,
	Cancelled,
	Suppressed,
	Interrupted,
	Failed
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHapticAsyncNativeTerminal,
	EOpenMobileHapticAsyncTerminalState,
	const FOpenMobileHapticPlaybackResult&
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticAsyncAccepted,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticAsyncStarted,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticAsyncCompleted,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticAsyncStopped,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticAsyncCancelled,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticAsyncSuppressed,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticAsyncInterrupted,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticAsyncFailed,
	const FOpenMobileHapticPlaybackResult&,
	Result
);

/**
 * This is the old raw-name task kept for Blueprints that already use it. It belongs to one Game Instance and you'll get one final event only, even if teardown and a native callback happen together.
 */
UCLASS(BlueprintType, Transient, meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILEHAPTICS_API UOpenMobileHapticPlaybackAsyncAction final :
	public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Accepted", ToolTip = "Broadcasts once when the named-pattern request is accepted and its playback handle is ready."))
	FOpenMobileHapticAsyncAccepted Accepted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Started", ToolTip = "Broadcasts when accepted playback starts. Inspect synchronization diagnostics when exact timing matters."))
	FOpenMobileHapticAsyncStarted Started;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Completed", ToolTip = "Broadcasts once when the accepted Haptics playback completes."))
	FOpenMobileHapticAsyncCompleted Completed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Stopped", ToolTip = "Broadcasts once when an owner stops accepted playback before its natural end."))
	FOpenMobileHapticAsyncStopped Stopped;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Cancelled", ToolTip = "Broadcasts once when pending or active Haptics playback is cancelled."))
	FOpenMobileHapticAsyncCancelled Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Suppressed", ToolTip = "Broadcasts once when the request is intentionally silent. Suppression never reaches Completed."))
	FOpenMobileHapticAsyncSuppressed Suppressed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Interrupted", ToolTip = "Broadcasts once when the operating system or native Haptics engine interrupts accepted playback."))
	FOpenMobileHapticAsyncInterrupted Interrupted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Failed", ToolTip = "Broadcasts once when the request is rejected or playback fails. Interruption and suppression use their own branches."))
	FOpenMobileHapticAsyncFailed Failed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Raw handle for accepted named-pattern playback."))
	FOpenMobileHapticPlaybackHandle PlaybackHandle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Immediate accepted, fallback, suppressed, or rejected submission result."))
	FOpenMobileHapticPlaybackResult ImmediateResult;

	/** Keeps existing raw-name graphs working, new code should use the typed named task so stale names are caught earlier. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", AutoCreateRefTerm = "Options", BlueprintInternalUseOnly = "true", DeprecatedFunction, DeprecationMessage = "Use Play Named Haptic with a typed configured identifier.", DisplayName = "Play Named Haptic Pattern Async (Legacy)", Keywords = "haptic named pattern legacy", ToolTip = "Legacy raw-name async playback retained for existing Blueprint assets.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPlaybackAsyncAction* PlayNamedHapticAsync(
		const UObject* WorldContextObject,
		FName PatternName,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	/** Submits only after Unreal has registered the task and Blueprint has bound every output delegate. */
	virtual void Activate() override;
	/** Moves pending or active work to Cancelled once, late playback events get ignored after that. */
	virtual void Cancel() override;

	/** Lets native callers check the terminal guard without inspecting Blueprint delegates. */
	bool IsFinished() const
	{
		return TerminalState != EOpenMobileHapticAsyncTerminalState::Pending;
	}

	/** Exposes the native-only final event used by subsystem contracts that can't bind a dynamic delegate. */
	FOpenMobileHapticAsyncNativeTerminal& OnNativeTerminal()
	{
		return NativeTerminal;
	}

private:
	friend class UOpenMobileHapticsSubsystem;
	friend class FOpenMobileHapticsAsyncContractTest;

	/** Seals the task on its first final state so competing engine callbacks can't broadcast twice. */
	bool TrySetTerminalState(EOpenMobileHapticAsyncTerminalState State);
	/** Publishes natural completion and includes the last request diagnostics for the caller. */
	void FinishCompleted(FOpenMobileHapticPlaybackResult Result);
	/** Keeps an owner-requested stop separate from natural completion. */
	void FinishStopped(FOpenMobileHapticPlaybackResult Result);
	/** Reports cancellation for pending and active work without turning it into native failure. */
	void FinishCancelled(FOpenMobileHapticPlaybackResult Result);
	/** Ends intentional silence on its own branch, no completion event follows it. */
	void FinishSuppressed(FOpenMobileHapticPlaybackResult Result);
	/** Preserves operating-system interruption as its own outcome so callers can decide whether to retry. */
	void FinishInterrupted(FOpenMobileHapticPlaybackResult Result);
	/** Sends validation, submission, or playback errors through the failure branch once. */
	void FinishFailed(FOpenMobileHapticPlaybackResult Result);
	/** Filters the subsystem event stream to the handle owned by this task. */
	void HandlePlaybackEvent(const FOpenMobileHapticPlaybackEvent& Event);
	/** Cancels while the world is still safe to reference and removes the cleanup hook straight away. */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	/** Stops unfinished work because the task's subsystem and handle registry are Game Instance scoped. */
	void HandleGameInstanceTeardown();
	/** Releases world and playback delegates after the task has no valid transition left. */
	void Cleanup();

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	TWeakObjectPtr<UOpenMobileHapticsSubsystem> Subsystem;
	TWeakObjectPtr<UWorld> TargetWorld;
	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle PlaybackEventHandle;
	FOpenMobileHapticAsyncNativeTerminal NativeTerminal;
	FName RequestedPatternName;
	float RequestedIntensity = 1.0f;
	FOpenMobileHapticPlaybackOptions RequestedOptions;
	EOpenMobileHapticAsyncTerminalState TerminalState =
		EOpenMobileHapticAsyncTerminalState::Pending;
};

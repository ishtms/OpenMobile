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
 * Async named-pattern request owned by its Game Instance until one terminal event.
 * Completion, cancellation, and failure are mutually exclusive and run on the
 * game thread. Game Instance teardown cancels unfinished work.
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

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Completed", ToolTip = "Broadcasts once when the accepted Haptics playback completes."))
	FOpenMobileHapticAsyncCompleted Completed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Stopped", ToolTip = "Broadcasts once when an owner stops accepted playback before its natural end."))
	FOpenMobileHapticAsyncStopped Stopped;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Cancelled", ToolTip = "Broadcasts once when pending or active Haptics playback is cancelled."))
	FOpenMobileHapticAsyncCancelled Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Suppressed", ToolTip = "Broadcasts once when the request is intentionally silent. Suppression never reaches Completed."))
	FOpenMobileHapticAsyncSuppressed Suppressed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Interrupted", ToolTip = "Broadcasts once when the operating system or native Haptics engine interrupts accepted playback."))
	FOpenMobileHapticAsyncInterrupted Interrupted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Failed", ToolTip = "Broadcasts once when the request is rejected or playback fails. Interruption and suppression use their own branches."))
	FOpenMobileHapticAsyncFailed Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackHandle PlaybackHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackResult ImmediateResult;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (AdvancedDisplay = "Options", BlueprintInternalUseOnly = "true", DisplayName = "Play Named Haptic Pattern Async", ToolTip = "Plays a named Haptics pattern and reports exactly one terminal event.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPlaybackAsyncAction* PlayNamedHapticAsync(
		const UObject* WorldContextObject,
		FName PatternName,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	virtual void Activate() override;
	virtual void Cancel() override;

	bool IsFinished() const
	{
		return TerminalState != EOpenMobileHapticAsyncTerminalState::Pending;
	}

	FOpenMobileHapticAsyncNativeTerminal& OnNativeTerminal()
	{
		return NativeTerminal;
	}

private:
	friend class UOpenMobileHapticsSubsystem;
	friend class FOpenMobileHapticsAsyncContractTest;

	bool TrySetTerminalState(EOpenMobileHapticAsyncTerminalState State);
	void FinishCompleted(FOpenMobileHapticPlaybackResult Result);
	void FinishStopped(FOpenMobileHapticPlaybackResult Result);
	void FinishCancelled(FOpenMobileHapticPlaybackResult Result);
	void FinishSuppressed(FOpenMobileHapticPlaybackResult Result);
	void FinishInterrupted(FOpenMobileHapticPlaybackResult Result);
	void FinishFailed(FOpenMobileHapticPlaybackResult Result);
	void HandlePlaybackEvent(const FOpenMobileHapticPlaybackEvent& Event);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandleGameInstanceTeardown();
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

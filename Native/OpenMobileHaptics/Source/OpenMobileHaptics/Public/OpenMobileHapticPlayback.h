#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"
#include "UObject/Object.h"
#include "OpenMobileHapticPlayback.generated.h"

class UOpenMobileHapticsSubsystem;
class UOpenMobileHapticPreparationLease;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticPlaybackAcceptedDynamic,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticPlaybackStartedDynamic,
	EOpenMobileHapticEventEvidence,
	Evidence
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHapticPlaybackFinishedDynamic,
	EOpenMobileHapticTerminalReason,
	Reason,
	const FOpenMobileHapticError&,
	Error
);

UCLASS(
	BlueprintType,
	Transient,
	meta = (DisplayName = "Haptic Playback")
)
class OPENMOBILEHAPTICS_API UOpenMobileHapticPlayback final : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Fires once after this playback object owns an accepted request."))
	FOpenMobileHapticPlaybackAcceptedDynamic OnAccepted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Fires when playback starts. Evidence states whether start was estimated, scheduler-confirmed, or native-confirmed."))
	FOpenMobileHapticPlaybackStartedDynamic OnStarted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Fires exactly once when accepted playback completes, stops, is cancelled, is interrupted, or fails."))
	FOpenMobileHapticPlaybackFinishedDynamic OnFinished;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Latest lifecycle state reported for this request."))
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Terminal reason after playback has finished. None means the request is not terminal."))
	EOpenMobileHapticTerminalReason TerminalReason =
		EOpenMobileHapticTerminalReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Semantic effect, game preset, or prepared pattern represented by this request."))
	FName PatternOrEffect;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Resolved project channel used by this playback."))
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "True when the accepted request used a lower-quality fallback path."))
	bool bUsedFallback = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Sanitized runtime path selected for this playback."))
	FName ResolvedQuality;

	/** Check this before issuing controls, because the object can stay referenced after its Game Instance has gone away. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Is Haptic Playback Valid", Keywords = "haptic handle valid active", ToolTip = "Returns true while this object owns a valid playback handle and its Game Instance is alive."))
	bool IsValid() const;

	/** Treats scheduled and paused requests as active too, since both still own playback state and can receive controls. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Is Haptic Playback Active", Keywords = "haptic playing scheduled", ToolTip = "Returns true while playback is accepted, scheduled, started, resumed, or paused."))
	bool IsActive() const;

	/** Uses the last reported state only, it won't guess that a failed pause actually took effect. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Is Haptic Playback Paused", Keywords = "haptic paused", ToolTip = "Returns true when the latest reported playback state is paused."))
	bool IsPaused() const;

	/** Requests a normal early finish, so listeners get Stopped when the resolved path can honour it. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Stop Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic end finish", ToolTip = "Gracefully stops this playback when its resolved path supports stop."))
	void Stop(
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Cancels ownership even before playback starts, which is the path to use when the request itself is no longer wanted. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Cancel Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic abort pending", ToolTip = "Cancels pending or active playback owned by this object."))
	void Cancel(
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Pauses only when the selected native or emulated path can preserve a useful resume position. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Pause Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic hold", ToolTip = "Pauses playback when its resolved path supports pause."))
	void Pause(
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Resumes from the position accepted by Pause, it won't restart a request that has already ended. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Resume Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic continue", ToolTip = "Resumes playback after a successful pause."))
	void Resume(
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Returns the position the backend could actually reach, because native playback may accept only fixed timing steps. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Seek Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic position timeline", ToolTip = "Moves playback to a non-negative timeline position and reports platform quantization."))
	void Seek(
		UPARAM(meta = (ClampMin = "0.0", Units = "s", ToolTip = "Requested playback position in seconds."))
		double PositionSeconds,
		EOpenMobileHapticControlBranch& Outcome,
		UPARAM(DisplayName = "Resolved Position", meta = (Units = "s", ToolTip = "Actual position selected after platform quantization."))
		double& ResolvedPositionSeconds,
		UPARAM(DisplayName = "Was Quantized", meta = (ToolTip = "True when the platform selected a nearby supported position."))
		bool& bWasQuantized,
		FOpenMobileHapticError& Error
	);

	/** Changes strength without resetting sharpness, and clamps wired Blueprint values before they reach native code. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Set Haptic Intensity", ExpandEnumAsExecs = "Outcome", Keywords = "haptic strength amplitude", ToolTip = "Updates only normalized playback intensity. Literal and wired values are clamped to the safe range."))
	void SetIntensity(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized playback intensity from zero to one."))
		float Intensity,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Changes sharpness without touching strength, unsupported paths return a typed failure instead of pretending it worked. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Set Haptic Sharpness", ExpandEnumAsExecs = "Outcome", Keywords = "haptic texture crisp soft", ToolTip = "Updates only normalized playback sharpness. Literal and wired values are clamped to the safe range."))
	void SetSharpness(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized sharpness. Zero is softer, 0.5 is neutral, and one is sharper."))
		float Sharpness,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Sends both values in one update so backends don't expose an avoidable half-updated frame. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Set Haptic Intensity And Sharpness", ExpandEnumAsExecs = "Outcome", Keywords = "haptic strength texture", ToolTip = "Updates normalized intensity and sharpness together. Literal and wired values are clamped to the safe range."))
	void SetIntensityAndSharpness(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized playback intensity from zero to one."))
		float Intensity,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized sharpness. Zero is softer, 0.5 is neutral, and one is sharper."))
		float Sharpness,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Detaches delegates and preparation ownership when garbage collection reaches a playback nobody retained. */
	virtual void BeginDestroy() override;

private:
	friend class UOpenMobileHapticsBlueprintLibrary;
	friend class UOpenMobileHapticPatternPlaybackAsyncAction;
	friend class UOpenMobileHapticNamedPlaybackAsyncAction;
	friend class UOpenMobileHapticsSubsystem;

	/** Copies the accepted request identity before any callback can arrive for this object. */
	void InitializePlayback(
		UOpenMobileHapticsSubsystem* InSubsystem,
		const FOpenMobileHapticPlaybackResult& Result,
		FName InPatternOrEffect
	);
	/** Keeps prepared content alive for exactly as long as playback may still ask the backend to use it. */
	void AttachPreparationLease(
		UOpenMobileHapticPreparationLease* InPreparationLease
	);
	/** Filters subsystem-wide events by handle so this object reflects its own request only. */
	void HandlePlaybackEvent(const FOpenMobileHapticPlaybackEvent& Event);
	/** Finishes safely when the owning Game Instance disappears before a native terminal callback. */
	void HandleGameInstanceTeardown();
	/** Delays Accepted till listeners can bind, then guarantees they see it once only. */
	void BroadcastAcceptedIfNeeded();
	/** Records one terminal reason before broadcasting and releasing all request ownership. */
	void Finish(
		EOpenMobileHapticTerminalReason Reason,
		const FOpenMobileHapticError& Error
	);
	/** Removes subsystem bindings and the preparation lease after this object can no longer receive useful events. */
	void Cleanup();
	/** Gives callers a stable stale-handle error instead of sending a control to an unrelated recycled request. */
	FOpenMobileHapticControlResult MakeStaleControlResult() const;
	/** Converts the native control result into Blueprint's two execution branches without losing the detailed error. */
	void ResolveControlResult(
		const FOpenMobileHapticControlResult& Result,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	TWeakObjectPtr<UOpenMobileHapticsSubsystem> Subsystem;
	FOpenMobileHapticPlaybackHandle Handle;
	FOpenMobileHapticPlaybackResult ImmediateResult;
	FDelegateHandle PlaybackEventHandle;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileHapticPreparationLease> PreparationLease;

	bool bAcceptedBroadcast = false;
	bool bFinished = false;
};

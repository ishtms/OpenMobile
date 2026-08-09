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

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Is Haptic Playback Valid", Keywords = "haptic handle valid active", ToolTip = "Returns true while this object owns a valid playback handle and its Game Instance is alive."))
	bool IsValid() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Is Haptic Playback Active", Keywords = "haptic playing scheduled", ToolTip = "Returns true while playback is accepted, scheduled, started, resumed, or paused."))
	bool IsActive() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Is Haptic Playback Paused", Keywords = "haptic paused", ToolTip = "Returns true when the latest reported playback state is paused."))
	bool IsPaused() const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Stop Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic end finish", ToolTip = "Gracefully stops this playback when its resolved path supports stop."))
	void Stop(
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Cancel Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic abort pending", ToolTip = "Cancels pending or active playback owned by this object."))
	void Cancel(
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Pause Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic hold", ToolTip = "Pauses playback when its resolved path supports pause."))
	void Pause(
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Resume Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic continue", ToolTip = "Resumes playback after a successful pause."))
	void Resume(
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

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

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Set Haptic Intensity", ExpandEnumAsExecs = "Outcome", Keywords = "haptic strength amplitude", ToolTip = "Updates only normalized playback intensity. Literal and wired values are clamped to the safe range."))
	void SetIntensity(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized playback intensity from zero to one."))
		float Intensity,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Set Haptic Sharpness", ExpandEnumAsExecs = "Outcome", Keywords = "haptic texture crisp soft", ToolTip = "Updates only normalized playback sharpness. Literal and wired values are clamped to the safe range."))
	void SetSharpness(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized sharpness. Zero is softer, 0.5 is neutral, and one is sharper."))
		float Sharpness,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Set Haptic Intensity And Sharpness", ExpandEnumAsExecs = "Outcome", Keywords = "haptic strength texture", ToolTip = "Updates normalized intensity and sharpness together. Literal and wired values are clamped to the safe range."))
	void SetIntensityAndSharpness(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized playback intensity from zero to one."))
		float Intensity,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized sharpness. Zero is softer, 0.5 is neutral, and one is sharper."))
		float Sharpness,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	virtual void BeginDestroy() override;

private:
	friend class UOpenMobileHapticsBlueprintLibrary;
	friend class UOpenMobileHapticPatternPlaybackAsyncAction;
	friend class UOpenMobileHapticNamedPlaybackAsyncAction;
	friend class UOpenMobileHapticsSubsystem;

	void InitializePlayback(
		UOpenMobileHapticsSubsystem* InSubsystem,
		const FOpenMobileHapticPlaybackResult& Result,
		FName InPatternOrEffect
	);
	void AttachPreparationLease(
		UOpenMobileHapticPreparationLease* InPreparationLease
	);
	void HandlePlaybackEvent(const FOpenMobileHapticPlaybackEvent& Event);
	void HandleGameInstanceTeardown();
	void BroadcastAcceptedIfNeeded();
	void Finish(
		EOpenMobileHapticTerminalReason Reason,
		const FOpenMobileHapticError& Error
	);
	void Cleanup();
	FOpenMobileHapticControlResult MakeStaleControlResult() const;
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

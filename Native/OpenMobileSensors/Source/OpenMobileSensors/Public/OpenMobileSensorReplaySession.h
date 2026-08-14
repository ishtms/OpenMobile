#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorReplaySession.generated.h"

class UOpenMobileSensorReplaySession;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileSensorReplaySessionStateDynamic,
	UOpenMobileSensorReplaySession*,
	Session,
	FOpenMobileSensorReplaySnapshot,
	Replay
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileSensorReplaySessionTerminalDynamic,
	UOpenMobileSensorReplaySession*,
	Session,
	FOpenMobileSensorReplayResult,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobileSensorReplaySessionFailureDynamic,
	UOpenMobileSensorReplaySession*,
	Session,
	FText,
	Message,
	FText,
	Correction,
	FOpenMobileSensorReplayResult,
	Details
);

UCLASS(BlueprintType, Transient, meta = (ExposedAsyncProxy = "Session"))
class OPENMOBILESENSORS_API UOpenMobileSensorReplaySession final
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Replay Started", ToolTip = "Broadcast once after the recording is decoded and replay is ready in its requested playing or paused state."))
	FOpenMobileSensorReplaySessionStateDynamic ReplayStarted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Paused", ToolTip = "Broadcast when this replay enters the paused state."))
	FOpenMobileSensorReplaySessionStateDynamic Paused;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Resumed", ToolTip = "Broadcast when this replay resumes from the paused state."))
	FOpenMobileSensorReplaySessionStateDynamic Resumed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "State Changed", ToolTip = "Broadcast whenever this session enters a different loading, playing, paused, or terminal state."))
	FOpenMobileSensorReplaySessionStateDynamic StateChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Looped", ToolTip = "Broadcast after a looping replay wraps from its end to its beginning."))
	FOpenMobileSensorReplaySessionStateDynamic Looped;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Completed", ToolTip = "Broadcast once after a non-looping replay publishes its final sample."))
	FOpenMobileSensorReplaySessionTerminalDynamic Completed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Failed", ToolTip = "Broadcast once when replay loading or playback fails, with a display message and correction."))
	FOpenMobileSensorReplaySessionFailureDynamic Failed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once after this replay is stopped, its owner is destroyed, or its world closes."))
	FOpenMobileSensorReplaySessionTerminalDynamic Cancelled;

	/** Use this when a recording needs owner-scoped playback and typed controls. The terminal events tell you whether it completed, failed, or was cancelled. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Replay Sensor File", Keywords = "OpenMobile sensors replay playback recording session", AdvancedDisplay = "SessionOwner", ToolTip = "Creates an owner-scoped replay session with typed controls and terminal events."))
	static UOpenMobileSensorReplaySession* ReplaySensorFile(
		const UObject* WorldContextObject,
		FString FilePath,
		FOpenMobileSensorReplayOptions Options,
		UObject* SessionOwner = nullptr
	);

	/** Use this to pause this replay without losing its playback position. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Pause Sensor Replay", ToolTip = "Pauses this replay without losing its playback position."))
	FOpenMobileSensorOperationResult PauseReplay();

	/** Use this to resume this paused replay. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Resume Sensor Replay", ToolTip = "Resumes this paused replay."))
	FOpenMobileSensorOperationResult ResumeReplay();

	/** Use this to move this replay to a recording-relative timespan. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Seek Sensor Replay To", ToolTip = "Moves this replay to a recording-relative timespan."))
	FOpenMobileSensorOperationResult SeekReplayTo(FTimespan PlaybackPosition);

	/** Use this to change playback speed while preserving the current position. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Set Sensor Replay Speed", ToolTip = "Changes playback speed while preserving the current position."))
	FOpenMobileSensorOperationResult SetReplaySpeed(double PlaybackSpeed);

	/** Use this to enable or disable looping for this replay. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Set Sensor Replay Looping", ToolTip = "Enables or disables looping for this replay."))
	FOpenMobileSensorOperationResult SetReplayLooping(bool bLoop);

	/** Use this only with a manual-clock replay when tests or tools must decide the exact step. Negative time won't be accepted. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Advance Manual Sensor Replay By", ToolTip = "Advanced deterministic control that advances a manual-clock replay by a nonnegative timespan."))
	FOpenMobileSensorOperationResult AdvanceReplayBy(FTimespan Delta);

	/** Call this when playback should end before completion. Calling it again is safe. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Stop Sensor Replay", ToolTip = "Stops this replay. Calling Stop more than once is safe."))
	FOpenMobileSensorOperationResult StopReplay();

	/** You'll get the session's cached replay state. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Sensor Replay State", ToolTip = "Returns the session's cached replay state."))
	EOpenMobileSensorReplayState GetReplayState() const;

	/** You'll get cached playback position, duration, speed, loop, clock, and state. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Sensor Replay Snapshot", ToolTip = "Returns cached playback position, duration, speed, loop, clock, and state."))
	FOpenMobileSensorReplaySnapshot GetReplaySnapshot() const;

	/** You'll get the cached recording-relative playback position as a timespan. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Sensor Replay Position", ToolTip = "Returns the cached recording-relative playback position as a timespan."))
	FTimespan GetReplayPosition() const;

	/** You'll get the decoded recording duration as a timespan. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Sensor Replay Duration", ToolTip = "Returns the decoded recording duration as a timespan."))
	FTimespan GetReplayDuration() const;

	/** You'll get true while this session is loading, playing, or paused. Completed and stopped sessions stay false. */
	virtual bool IsActive() const override;

	/** Unreal calls this after Replay Sensor File returns the session. It binds owner lifetime before loading the recording. */
	virtual void Activate() override;

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests call this to advance owner and replay work without a running ticker. Shipping builds don't expose it. */
	bool TickForTests();
#endif

protected:
	/** This stops unfinished playback when owner teardown cancels the session. A terminal replay has nothing left to cancel. */
	virtual void CancelNativeOperation() override;

	/** This publishes the typed replay event matching the latest native state. */
	virtual void OnActionSucceeded() override;

	/** This publishes terminal replay failure and keeps the last coherent snapshot available. */
	virtual void OnActionFailed(const FOpenMobileError& Error) override;

	/** This publishes cancellation once even if a decoder or clock callback arrives later. */
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	FOpenMobileSensorOperationResult InvalidSessionResult() const;
	void HandleComplete(const FOpenMobileSensorReplayResult& InResult);
	bool TickSession(float DeltaSeconds);
	void RefreshSnapshot(bool bBroadcastTransitions);
	void Unbind();

	UPROPERTY(Transient)
	TObjectPtr<UObject> ActivationWorldContext;

	TWeakObjectPtr<UObject> LifetimeOwner;
	FString FilePath;
	FOpenMobileSensorReplayOptions Options;
	FGuid RequestId;
	FOpenMobileSensorReplaySnapshot Snapshot;
	FOpenMobileSensorReplayResult Result;
	FTSTicker::FDelegateHandle SessionTickerHandle;
	bool bStartedBroadcast = false;
};

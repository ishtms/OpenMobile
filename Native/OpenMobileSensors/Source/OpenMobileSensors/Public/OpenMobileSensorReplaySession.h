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

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Replay Sensor File", Keywords = "OpenMobile sensors replay playback recording session", AdvancedDisplay = "SessionOwner", ToolTip = "Creates an owner-scoped replay session with typed controls and terminal events."))
	static UOpenMobileSensorReplaySession* ReplaySensorFile(
		const UObject* WorldContextObject,
		FString FilePath,
		FOpenMobileSensorReplayOptions Options,
		UObject* SessionOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Pause Sensor Replay", ToolTip = "Pauses this replay without losing its playback position."))
	FOpenMobileSensorOperationResult PauseReplay();

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Resume Sensor Replay", ToolTip = "Resumes this paused replay."))
	FOpenMobileSensorOperationResult ResumeReplay();

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Seek Sensor Replay To", ToolTip = "Moves this replay to a recording-relative timespan."))
	FOpenMobileSensorOperationResult SeekReplayTo(FTimespan PlaybackPosition);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Set Sensor Replay Speed", ToolTip = "Changes playback speed while preserving the current position."))
	FOpenMobileSensorOperationResult SetReplaySpeed(double PlaybackSpeed);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Set Sensor Replay Looping", ToolTip = "Enables or disables looping for this replay."))
	FOpenMobileSensorOperationResult SetReplayLooping(bool bLoop);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Advance Manual Sensor Replay By", ToolTip = "Advanced deterministic control that advances a manual-clock replay by a nonnegative timespan."))
	FOpenMobileSensorOperationResult AdvanceReplayBy(FTimespan Delta);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Stop Sensor Replay", ToolTip = "Stops this replay. Calling Stop more than once is safe."))
	FOpenMobileSensorOperationResult StopReplay();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Sensor Replay State", ToolTip = "Returns the session's cached replay state."))
	EOpenMobileSensorReplayState GetReplayState() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Sensor Replay Snapshot", ToolTip = "Returns cached playback position, duration, speed, loop, clock, and state."))
	FOpenMobileSensorReplaySnapshot GetReplaySnapshot() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Sensor Replay Position", ToolTip = "Returns the cached recording-relative playback position as a timespan."))
	FTimespan GetReplayPosition() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Sensor Replay Duration", ToolTip = "Returns the decoded recording duration as a timespan."))
	FTimespan GetReplayDuration() const;

	virtual bool IsActive() const override;
	virtual void Activate() override;

#if WITH_DEV_AUTOMATION_TESTS
	bool TickForTests();
#endif

protected:
	virtual void CancelNativeOperation() override;
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
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

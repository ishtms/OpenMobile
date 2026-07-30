#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorRecordingSession.generated.h"

class UOpenMobileSensorRecordingSession;
class UOpenMobileSensorListener;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileSensorRecordingSessionStateDynamic,
	UOpenMobileSensorRecordingSession*,
	Session,
	FOpenMobileSensorRecordingSnapshot,
	Recording,
	FOpenMobileSensorRecordingOptions,
	AppliedOptions
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileSensorRecordingSessionLimitDynamic,
	UOpenMobileSensorRecordingSession*,
	Session,
	EOpenMobileSensorRecordingLimitReason,
	Limit,
	FOpenMobileSensorRecordingSnapshot,
	Recording
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobileSensorRecordingSessionFailureDynamic,
	UOpenMobileSensorRecordingSession*,
	Session,
	FText,
	Message,
	FText,
	Correction,
	FOpenMobileSensorRecordingResult,
	Details
);

UCLASS(BlueprintType, Transient, meta = (ExposedAsyncProxy = "Session"))
class OPENMOBILESENSORS_API UOpenMobileSensorRecordingSession final
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Recording Started", ToolTip = "Broadcast once after the file and hidden sensor streams are ready. The recording is active and is not finalized yet."))
	FOpenMobileSensorRecordingSessionStateDynamic RecordingStarted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Finalized", ToolTip = "Broadcast once after a complete recording footer is written and the final file is ready for replay."))
	FOpenMobileSensorRecordingSessionStateDynamic Finalized;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Limit Reached", ToolTip = "Broadcast before Finalized when the configured duration or file-size limit ends the recording."))
	FOpenMobileSensorRecordingSessionLimitDynamic LimitReached;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Failed", ToolTip = "Broadcast once when recording or finalization fails, with a display message and correction."))
	FOpenMobileSensorRecordingSessionFailureDynamic Failed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once after the recording is discarded. A partial or committed recording file is removed."))
	FOpenMobileSensorRecordingSessionStateDynamic Cancelled;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Record Sensors with Options (Advanced)", Keywords = "OpenMobile sensors record session capture file raw options", AdvancedDisplay = "SessionOwner", ToolTip = "Advanced options-based recording path. The sensor list must not be empty and every limit must satisfy Project Settings policy. Recording Started means capture is active, while Finalized means the complete file is ready."))
	static UOpenMobileSensorRecordingSession* RecordSensors(
		const UObject* WorldContextObject,
		FOpenMobileSensorRecordingOptions Options,
		UObject* SessionOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Recording Sensor", Keywords = "OpenMobile sensors record one capture file", AdvancedDisplay = "SessionOwner", ToolTip = "Starts an owner-scoped recording for the preferred instance of one sensor using Project Settings limits."))
	static UOpenMobileSensorRecordingSession* StartRecordingSensor(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor,
		UObject* SessionOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Recording Listeners", Keywords = "OpenMobile sensors record listeners selected capture file", AutoCreateRefTerm = "Listeners", AdvancedDisplay = "SessionOwner", ToolTip = "Starts an owner-scoped recording for the distinct sensors selected by typed listeners, using Project Settings limits."))
	static UOpenMobileSensorRecordingSession* StartRecordingListeners(
		const UObject* WorldContextObject,
		const TArray<UOpenMobileSensorListener*>& Listeners,
		UObject* SessionOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Recording Active Sensors", Keywords = "OpenMobile sensors record active listeners streams capture file", AdvancedDisplay = "SessionOwner", ToolTip = "Starts an owner-scoped recording for every distinct sensor currently starting, active, or paused in this Game Instance, using Project Settings limits."))
	static UOpenMobileSensorRecordingSession* StartRecordingActiveSensors(
		const UObject* WorldContextObject,
		UObject* SessionOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Finalize Sensor Recording", Keywords = "OpenMobile sensors recording stop save finish", ToolTip = "Stops capture and writes a complete recording file that can be replayed."))
	void FinalizeRecording();

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Discard Sensor Recording", Keywords = "OpenMobile sensors recording cancel delete discard", ToolTip = "Cancels capture and deletes the partial or committed recording file."))
	void DiscardRecording();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Recording State", ToolTip = "Returns the session's cached recording state."))
	EOpenMobileSensorRecordingState GetRecordingState() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Recording Snapshot", ToolTip = "Returns the latest cached file path, duration, size, drops, and state."))
	FOpenMobileSensorRecordingSnapshot GetRecordingSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Recording Limit Reason", ToolTip = "Returns the duration or file-size limit that ended this session, or No Limit."))
	EOpenMobileSensorRecordingLimitReason GetLimitReason() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Applied Recording Options", ToolTip = "Returns the distinct recorded sensors and the Project Settings limits applied to this session."))
	FOpenMobileSensorRecordingOptions GetAppliedRecordingOptions() const;

	virtual bool IsActive() const override;
	virtual void Activate() override;

protected:
	virtual void CancelNativeOperation() override;
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void HandleStart(const FOpenMobileSensorRecordingResult& InResult);
	void HandleFinalize(const FOpenMobileSensorRecordingResult& InResult);
	void HandleTerminated(
		const FGuid& InRequestId,
		const FOpenMobileSensorRecordingResult& InResult,
		EOpenMobileSensorRecordingLimitReason InLimitReason
	);
	bool TickOwner(float DeltaSeconds);
	void Unbind();
	void FinishFromResult(const FOpenMobileSensorRecordingResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> ActivationWorldContext;

	TWeakObjectPtr<UObject> LifetimeOwner;
	FOpenMobileSensorRecordingOptions Options;
	FGuid RequestId;
	FOpenMobileSensorRecordingResult Result;
	EOpenMobileSensorRecordingLimitReason LimitReason =
		EOpenMobileSensorRecordingLimitReason::None;
	FDelegateHandle TerminatedHandle;
	FTSTicker::FDelegateHandle OwnerTickerHandle;
	bool bFinalizeRequested = false;
	bool bRecordActiveSensors = false;
};

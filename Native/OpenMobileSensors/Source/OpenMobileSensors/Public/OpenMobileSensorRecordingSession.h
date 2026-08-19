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

	/** Use this when the recording needs a custom sensor list or custom limits. Recording Started means capture is active, and Finalized is the point where the complete file is ready. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Record Sensors with Options (Advanced)", Keywords = "OpenMobile sensors record session capture file raw options", AdvancedDisplay = "SessionOwner", ToolTip = "Advanced options-based recording for vector motion sensors. Unsupported sensor types fail the whole request and are named in the error. The sensor list must not be empty and every limit must satisfy Project Settings policy. Recording Started means capture is active, while Finalized means the complete file is ready."))
	static UOpenMobileSensorRecordingSession* RecordSensors(
		const UObject* WorldContextObject,
		FOpenMobileSensorRecordingOptions Options,
		UObject* SessionOwner = nullptr
	);

	/** Use this when one preferred sensor should be recorded with the Project Settings limits. The returned session owns finalization and cleanup. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Recording Sensor", Keywords = "OpenMobile sensors record one capture file", AdvancedDisplay = "SessionOwner", ToolTip = "Records the preferred instance of one vector motion sensor using Project Settings limits."))
	static UOpenMobileSensorRecordingSession* StartRecordingSensor(
		const UObject* WorldContextObject,
		UPARAM(meta = (ValidEnumValues = "Accelerometer,AccelerometerUncalibrated,Gyroscope,GyroscopeUncalibrated,Magnetometer,MagnetometerUncalibrated,Gravity,LinearAcceleration")) EOpenMobileSensorType Sensor,
		UObject* SessionOwner = nullptr
	);

	/** Use this when the sensors already belong to typed listeners. Repeated sensor selections are recorded once only, and Project Settings still cap the file. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Recording Listeners", Keywords = "OpenMobile sensors record listeners selected capture file", AutoCreateRefTerm = "Listeners", AdvancedDisplay = "SessionOwner", ToolTip = "Records distinct vector motion sensors selected by typed listeners using Project Settings limits. Any unsupported sensor fails the whole request and is named in the error. Use Is Sensor Recordable to check selections."))
	static UOpenMobileSensorRecordingSession* StartRecordingListeners(
		const UObject* WorldContextObject,
		const TArray<UOpenMobileSensorListener*>& Listeners,
		UObject* SessionOwner = nullptr
	);

	/** Records the active selection only when every sensor supports vector recording. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Recording Active Sensors", Keywords = "OpenMobile sensors record active listeners streams capture file", AdvancedDisplay = "SessionOwner", ToolTip = "Records every distinct starting, active, or paused sensor in this Game Instance using Project Settings limits. Only vector motion sensors are supported. Any unsupported sensor fails the whole request and is named in the error."))
	static UOpenMobileSensorRecordingSession* StartRecordingActiveSensors(
		const UObject* WorldContextObject,
		UObject* SessionOwner = nullptr
	);

	/** Call this when capture should stop and the replayable file should be written. Wait for Finalized before opening that file. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Finalize Sensor Recording", Keywords = "OpenMobile sensors recording stop save finish", ToolTip = "Stops capture and writes a complete recording file that can be replayed."))
	void FinalizeRecording();

	/** Call this when the recording shouldn't be kept. It cancels capture and removes the partial or committed file. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Discard Sensor Recording", Keywords = "OpenMobile sensors recording cancel delete discard", ToolTip = "Cancels capture and deletes the partial or committed recording file."))
	void DiscardRecording();

	/** You'll get the session's cached recording state. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Recording State", ToolTip = "Returns the session's cached recording state."))
	EOpenMobileSensorRecordingState GetRecordingState() const;

	/** You'll get the latest cached file path, duration, size, drops, and state. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Recording Snapshot", ToolTip = "Returns the latest cached file path, duration, size, drops, and state."))
	FOpenMobileSensorRecordingSnapshot GetRecordingSnapshot() const;

	/** You'll get the duration or file-size limit that ended this session, or No Limit. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Recording Limit Reason", ToolTip = "Returns the duration or file-size limit that ended this session, or No Limit."))
	EOpenMobileSensorRecordingLimitReason GetLimitReason() const;

	/** You'll get the different recorded sensors and the Project Settings limits applied to this session. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Applied Recording Options", ToolTip = "Returns the distinct recorded sensors and the Project Settings limits applied to this session."))
	FOpenMobileSensorRecordingOptions GetAppliedRecordingOptions() const;

	/** You'll get true while this session is starting, recording, or finalizing. Terminal sessions stay false even if their UObject still exists. */
	virtual bool IsActive() const override;

	/** Unreal calls this after the session factory returns. It starts capture once and binds owner lifetime before samples can arrive. */
	virtual void Activate() override;

protected:
	/** This discards unfinished native capture when owner teardown cancels the session. A requested finalization is allowed to finish instead. */
	virtual void CancelNativeOperation() override;

	/** This publishes the correct recording event after the base accepts success. Start and finalization stay separate. */
	virtual void OnActionSucceeded() override;

	/** This publishes terminal failure with the latest file and limit context. It won't claim the file is finalized. */
	virtual void OnActionFailed(const FOpenMobileError& Error) override;

	/** This publishes cancellation once after native capture has been discarded. */
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

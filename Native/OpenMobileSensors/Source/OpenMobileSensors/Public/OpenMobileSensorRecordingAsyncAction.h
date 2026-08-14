#pragma once

#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorRecordingAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileSensorRecordingAsyncResult,
	FOpenMobileSensorRecordingResult,
	Result
);

UENUM()
enum class EOpenMobileSensorRecordingAsyncOperation : uint8
{
	Start,
	Stop
};

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILESENSORS_API UOpenMobileSensorRecordingAsyncAction final
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (DisplayName = "Recording Operation Completed", ToolTip = "For start, this means capture became active. For stop, this means the final file is ready. Prefer the typed Record Sensors session for lifecycle events."))
	FOpenMobileSensorRecordingAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when the recording operation is cancelled."))
	FOpenMobileSensorRecordingAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when the recording operation fails."))
	FOpenMobileSensorRecordingAsyncResult Failed;

	/** Use this only for raw GUID-controlled recording. Completion means capture started, the file isn't ready till finalization. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Sensor Recording (Advanced)", ToolTip = "Starts a raw recording request. Its completion means recording started, not that the file was finalized. Prefer Record Sensors for lifetime control."))
	static UOpenMobileSensorRecordingAsyncAction* StartSensorRecording(
		const UObject* WorldContextObject,
		FOpenMobileSensorRecordingOptions Options
	);

	/** Use this to finalize a raw recording by GUID. Typed sessions should call Finalize Sensor Recording instead. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Stop Sensor Recording (Advanced)", ToolTip = "Stops and finalizes one raw recording request by GUID. Prefer Finalize Sensor Recording on a typed session."))
	static UOpenMobileSensorRecordingAsyncAction* StopSensorRecording(
		const UObject* WorldContextObject,
		FGuid RecordingRequestId
	);

	/** Unreal calls this after the raw recording node is wired. It starts or finalizes the one request stored by the factory. */
	virtual void Activate() override;

protected:
	/** This cancels the unfinished raw recording request during owner teardown. Partial output won't be presented as finalized. */
	virtual void CancelNativeOperation() override;

	/** This broadcasts the captured raw recording result after terminal state is secured. */
	virtual void OnActionSucceeded() override;

	/** This broadcasts recording failure with the same typed payload returned by the subsystem. */
	virtual void OnActionFailed(const FOpenMobileError& Error) override;

	/** This broadcasts cancellation once and ignores any later writer callback. */
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void HandleComplete(const FOpenMobileSensorRecordingResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	EOpenMobileSensorRecordingAsyncOperation Operation =
		EOpenMobileSensorRecordingAsyncOperation::Start;
	FOpenMobileSensorRecordingOptions Options;
	FGuid RecordingRequestId;
	FOpenMobileSensorRecordingResult Result;
};

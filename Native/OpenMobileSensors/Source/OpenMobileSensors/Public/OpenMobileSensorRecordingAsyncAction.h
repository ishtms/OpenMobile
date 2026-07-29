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

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Sensor Recording (Advanced)", ToolTip = "Starts a raw recording request. Its completion means recording started, not that the file was finalized. Prefer Record Sensors for lifetime control."))
	static UOpenMobileSensorRecordingAsyncAction* StartSensorRecording(
		const UObject* WorldContextObject,
		FOpenMobileSensorRecordingOptions Options
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Stop Sensor Recording (Advanced)", ToolTip = "Stops and finalizes one raw recording request by GUID. Prefer Finalize Sensor Recording on a typed session."))
	static UOpenMobileSensorRecordingAsyncAction* StopSensorRecording(
		const UObject* WorldContextObject,
		FGuid RecordingRequestId
	);

	virtual void Activate() override;

protected:
	virtual void CancelNativeOperation() override;
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
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

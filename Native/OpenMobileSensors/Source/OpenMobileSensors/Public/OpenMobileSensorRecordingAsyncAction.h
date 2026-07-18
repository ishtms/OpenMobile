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
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Completed", ToolTip = "Broadcast once when the recording operation completes."))
	FOpenMobileSensorRecordingAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when the recording operation is cancelled."))
	FOpenMobileSensorRecordingAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when the recording operation fails."))
	FOpenMobileSensorRecordingAsyncResult Failed;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Start Sensor Recording", ToolTip = "Starts a bounded sensor recording and completes exactly once."))
	static UOpenMobileSensorRecordingAsyncAction* StartSensorRecording(
		const UObject* WorldContextObject,
		FOpenMobileSensorRecordingOptions Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Stop Sensor Recording", ToolTip = "Stops and finalizes one accepted sensor recording."))
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

#pragma once

#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorReplayAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileSensorReplayAsyncResult,
	FOpenMobileSensorReplayResult,
	Result
);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILESENSORS_API UOpenMobileSensorReplayAsyncAction final
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Completed", ToolTip = "Broadcast once when the raw replay request reaches its terminal result."))
	FOpenMobileSensorReplayAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when sensor replay is cancelled."))
	FOpenMobileSensorReplayAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when sensor replay cannot start or continue."))
	FOpenMobileSensorReplayAsyncResult Failed;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Replay Sensor Recording by GUID (Advanced)", ToolTip = "Starts a raw GUID-controlled replay. Prefer Replay Sensor File for typed state and controls."))
	static UOpenMobileSensorReplayAsyncAction* ReplaySensorRecording(
		const UObject* WorldContextObject,
		FString FilePath,
		FOpenMobileSensorReplayOptions Options
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Get Sensor Replay Request ID (Advanced)", ToolTip = "Returns the raw request GUID used by advanced subsystem replay controls after this action activates."))
	FGuid GetReplayRequestId() const
	{
		return RequestId;
	}

	virtual void Activate() override;

protected:
	virtual void CancelNativeOperation() override;
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void HandleComplete(const FOpenMobileSensorReplayResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FString FilePath;
	FOpenMobileSensorReplayOptions Options;
	FGuid RequestId;
	FOpenMobileSensorReplayResult Result;
};

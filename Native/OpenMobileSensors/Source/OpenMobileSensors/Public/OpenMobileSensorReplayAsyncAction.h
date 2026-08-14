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

	/** Use this only when you need raw GUID-controlled replay. Replay Sensor File gives normal Blueprint graphs typed state and controls. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Replay Sensor Recording by GUID (Advanced)", ToolTip = "Starts a raw GUID-controlled replay. Prefer Replay Sensor File for typed state and controls."))
	static UOpenMobileSensorReplayAsyncAction* ReplaySensorRecording(
		const UObject* WorldContextObject,
		FString FilePath,
		FOpenMobileSensorReplayOptions Options
	);

	/** You'll need this GUID only for the advanced subsystem replay controls. It isn't valid before the action activates. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Get Sensor Replay Request ID (Advanced)", ToolTip = "Returns the raw request GUID used by advanced subsystem replay controls after this action activates."))
	FGuid GetReplayRequestId() const
	{
		return RequestId;
	}

	/** Unreal calls this after the raw replay node is wired. It submits the stored file and replay options once. */
	virtual void Activate() override;

protected:
	/** This cancels unfinished raw playback during owner teardown. A missing request ID is harmless. */
	virtual void CancelNativeOperation() override;

	/** This broadcasts the captured replay result after terminal state is secured. */
	virtual void OnActionSucceeded() override;

	/** This broadcasts replay failure without letting a late completion fire too. */
	virtual void OnActionFailed(const FOpenMobileError& Error) override;

	/** This broadcasts cancellation once even if decoding finishes later. */
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

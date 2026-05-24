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
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Completed", ToolTip = "Broadcast once when replay reaches its terminal result."))
	FOpenMobileSensorReplayAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when sensor replay is cancelled."))
	FOpenMobileSensorReplayAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when sensor replay cannot start or continue."))
	FOpenMobileSensorReplayAsyncResult Failed;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Replay Sensor Recording", ToolTip = "Replays a sensor recording through the common processing path."))
	static UOpenMobileSensorReplayAsyncAction* ReplaySensorRecording(
		const UObject* WorldContextObject,
		FString FilePath,
		FOpenMobileSensorReplayOptions Options
	);

	virtual void Activate() override;

protected:
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void HandleComplete(const FOpenMobileSensorReplayResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FString FilePath;
	FOpenMobileSensorReplayOptions Options;
	FOpenMobileSensorReplayResult Result;
};

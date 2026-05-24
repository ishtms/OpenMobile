#pragma once

#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorFlushAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileSensorFlushAsyncResult,
	FOpenMobileSensorFlushResult,
	Result
);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILESENSORS_API UOpenMobileSensorFlushAsyncAction final
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Completed", ToolTip = "Broadcast once after accepted native and plugin buffers are drained."))
	FOpenMobileSensorFlushAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when the flush is cancelled."))
	FOpenMobileSensorFlushAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when the flush cannot complete."))
	FOpenMobileSensorFlushAsyncResult Failed;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Flush Sensor Samples", ToolTip = "Flushes one subscription and completes exactly once on the game thread."))
	static UOpenMobileSensorFlushAsyncAction* FlushSensorSamples(
		const UObject* WorldContextObject,
		FOpenMobileSensorSubscriptionHandle Handle
	);

	virtual void Activate() override;

protected:
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void HandleComplete(const FOpenMobileSensorFlushResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FOpenMobileSensorSubscriptionHandle Handle;
	FOpenMobileSensorFlushResult Result;
};

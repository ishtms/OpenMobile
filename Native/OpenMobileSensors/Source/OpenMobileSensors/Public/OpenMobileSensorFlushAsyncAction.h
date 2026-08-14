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
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Completed", ToolTip = "Broadcast once after accepted native and plugin buffers are drained."))
	FOpenMobileSensorFlushAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when the flush is cancelled."))
	FOpenMobileSensorFlushAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when the flush cannot complete."))
	FOpenMobileSensorFlushAsyncResult Failed;

	/** Use this when buffered native samples must be delivered before you continue. Completion fires once on the game thread only. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Flush Sensor Samples", ToolTip = "Flushes one subscription and completes exactly once on the game thread."))
	static UOpenMobileSensorFlushAsyncAction* FlushSensorSamples(
		const UObject* WorldContextObject,
		FOpenMobileSensorSubscriptionHandle Handle
	);

	/** Unreal calls this after the async node is wired. It submits one flush for the stored subscription. */
	virtual void Activate() override;

protected:
	/** This cancels the outstanding flush when the action or its world ends. No request ID means there's nothing to cancel. */
	virtual void CancelNativeOperation() override;

	/** This broadcasts the captured flush result after the base wins terminal state. */
	virtual void OnActionSucceeded() override;

	/** This broadcasts typed flush failure without allowing a later callback to fire again. */
	virtual void OnActionFailed(const FOpenMobileError& Error) override;

	/** This broadcasts cancellation once and keeps any late provider completion quiet. */
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void HandleComplete(const FOpenMobileSensorFlushResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FOpenMobileSensorSubscriptionHandle Handle;
	FGuid RequestId;
	FOpenMobileSensorFlushResult Result;
};

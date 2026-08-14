#pragma once

#include "OpenMobileNativeStepCount.h"
#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileNativeStepCountAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileNativeStepCountAsyncResult,
	FOpenMobileNativeStepCountQueryResult,
	Result
);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILESENSORS_API UOpenMobileNativeStepCountAsyncAction final
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Completed", ToolTip = "Broadcast once with the historical native step total."))
	FOpenMobileNativeStepCountAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when the historical step query is cancelled."))
	FOpenMobileNativeStepCountAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when the historical step query fails."))
	FOpenMobileNativeStepCountAsyncResult Failed;

	/** Use this when you need to query the platform native step total for a historical Unix-time interval when supported. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Query Native Step Count", ToolTip = "Queries the platform native step total for a historical Unix-time interval when supported."))
	static UOpenMobileNativeStepCountAsyncAction* QueryNativeStepCount(
		const UObject* WorldContextObject,
		FOpenMobileNativeStepCountQuery Query
	);

	/** Unreal calls this after the async node is wired. It submits one historical query only. */
	virtual void Activate() override;

protected:
	/** This cancels the outstanding historical query when the action or its world ends. No request ID means there's nothing to cancel. */
	virtual void CancelNativeOperation() override;

	/** This broadcasts the completed query result after the base wins terminal state. */
	virtual void OnActionSucceeded() override;

	/** This broadcasts typed query failure using the result already captured from the subsystem. */
	virtual void OnActionFailed(const FOpenMobileError& Error) override;

	/** This broadcasts cancellation once and keeps any late native callback quiet. */
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void HandleComplete(const FOpenMobileNativeStepCountQueryResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FOpenMobileNativeStepCountQuery Query;
	FGuid RequestId;
	FOpenMobileNativeStepCountQueryResult Result;
};

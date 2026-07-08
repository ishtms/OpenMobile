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
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Completed", ToolTip = "Broadcast once with the historical native step total."))
	FOpenMobileNativeStepCountAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when the historical step query is cancelled."))
	FOpenMobileNativeStepCountAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when the historical step query fails."))
	FOpenMobileNativeStepCountAsyncResult Failed;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Query Native Step Count", ToolTip = "Queries the platform native step total for a historical Unix-time interval when supported."))
	static UOpenMobileNativeStepCountAsyncAction* QueryNativeStepCount(
		const UObject* WorldContextObject,
		FOpenMobileNativeStepCountQuery Query
	);

	virtual void Activate() override;

protected:
	virtual void CancelNativeOperation() override;
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void HandleComplete(const FOpenMobileNativeStepCountQueryResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FOpenMobileNativeStepCountQuery Query;
	FGuid RequestId;
	FOpenMobileNativeStepCountQueryResult Result;
};

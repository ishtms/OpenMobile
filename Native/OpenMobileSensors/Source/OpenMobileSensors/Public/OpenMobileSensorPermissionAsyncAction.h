#pragma once

#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorPermissionAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileSensorPermissionAsyncResult,
	FOpenMobilePermissionResult,
	Result
);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILESENSORS_API UOpenMobileSensorPermissionAsyncAction final
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Completed", ToolTip = "Broadcast once with the normalized permission decision."))
	FOpenMobileSensorPermissionAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when the permission request is cancelled."))
	FOpenMobileSensorPermissionAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when the permission request cannot complete."))
	FOpenMobileSensorPermissionAsyncResult Failed;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Request Sensor Permission", ToolTip = "Requests one sensor-owned permission and completes exactly once on the game thread."))
	static UOpenMobileSensorPermissionAsyncAction* RequestSensorPermission(
		const UObject* WorldContextObject,
		EOpenMobileSensorPermission Permission
	);

	virtual void Activate() override;

protected:
	virtual void CancelNativeOperation() override;
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FOpenMobileSensorsPermissionOwnerTeardownTest;
#endif
	void HandleComplete(const FOpenMobilePermissionResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	EOpenMobileSensorPermission Permission =
		EOpenMobileSensorPermission::MotionActivity;
	FOpenMobilePermissionRequestHandle RequestHandle;
	FOpenMobilePermissionResult Result;
};

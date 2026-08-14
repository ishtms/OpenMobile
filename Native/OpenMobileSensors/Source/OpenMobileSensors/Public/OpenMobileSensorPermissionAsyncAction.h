#pragma once

#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorIdentifiers.h"
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
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Completed", ToolTip = "Broadcast once with the normalized permission decision."))
	FOpenMobileSensorPermissionAsyncResult Completed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Cancelled", ToolTip = "Broadcast once when the permission request is cancelled."))
	FOpenMobileSensorPermissionAsyncResult Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast once with typed details when the permission request cannot complete."))
	FOpenMobileSensorPermissionAsyncResult Failed;

	/** Call this from a user action when you already know the sensor-owned permission. Completion fires once on the game thread only. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Request Sensor Permission", ToolTip = "Requests one sensor-owned permission and completes exactly once on the game thread."))
	static UOpenMobileSensorPermissionAsyncAction* RequestSensorPermission(
		const UObject* WorldContextObject,
		EOpenMobileSensorPermission Permission
	);

	/** Call this from a user action and it'll work out what the sensor needs. No permission completes immediately, while location stays an external prerequisite and won't open a prompt here. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Permissions", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Request Access Needed by Sensor", Keywords = "OpenMobile sensors permission access prompt activity heading", ToolTip = "Call from a user-initiated action. Requests the permission needed by this sensor, completes immediately when no permission is needed, and reports external prerequisites without opening a prompt."))
	static UOpenMobileSensorPermissionAsyncAction* RequestAccessNeededBySensor(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor
	);

	/** Unreal calls this after the async node is wired. It resolves or requests the stored permission once only. */
	virtual void Activate() override;

protected:
	/** This cancels an unfinished platform permission request during owner teardown. A completed prompt won't be reopened. */
	virtual void CancelNativeOperation() override;

	/** This broadcasts the normalized permission result after terminal state is secured. */
	virtual void OnActionSucceeded() override;

	/** This broadcasts a request failure using the captured permission payload. */
	virtual void OnActionFailed(const FOpenMobileError& Error) override;

	/** This broadcasts cancellation once even if the platform answers later. */
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FOpenMobileSensorsPermissionOwnerTeardownTest;
	friend class FOpenMobileSensorsRequestAccessBySensorTest;
#endif
	void HandleComplete(const FOpenMobilePermissionResult& InResult);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	EOpenMobileSensorPermission Permission =
		EOpenMobileSensorPermission::MotionActivity;
	EOpenMobileSensorType Sensor = EOpenMobileSensorType::Accelerometer;
	bool bResolvePermissionFromSensor = false;
	FOpenMobilePermissionRequestHandle RequestHandle;
	FOpenMobilePermissionResult Result;
};

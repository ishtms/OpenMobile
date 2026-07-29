#include "OpenMobileSensorPermissionAsyncAction.h"

#include "OpenMobileSensorDiscoveryLibrary.h"
#include "OpenMobileSensorsSubsystem.h"

UOpenMobileSensorPermissionAsyncAction*
UOpenMobileSensorPermissionAsyncAction::RequestSensorPermission(
	const UObject* WorldContextObject,
	EOpenMobileSensorPermission Permission
)
{
	UOpenMobileSensorPermissionAsyncAction* Action =
		NewObject<UOpenMobileSensorPermissionAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Permission = Permission;
	Action->Result.Permission =
		FOpenMobileSensorPermissions::GetPermissionName(Permission);
	return Action;
}

UOpenMobileSensorPermissionAsyncAction*
UOpenMobileSensorPermissionAsyncAction::RequestAccessNeededBySensor(
	const UObject* WorldContextObject,
	EOpenMobileSensorType Sensor)
{
	UOpenMobileSensorPermissionAsyncAction* Action =
		NewObject<UOpenMobileSensorPermissionAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Sensor = Sensor;
	Action->bResolvePermissionFromSensor = true;
	return Action;
}

void UOpenMobileSensorPermissionAsyncAction::Activate()
{
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}
	if (bResolvePermissionFromSensor)
	{
		const FOpenMobileSensorAccessRequirement Access =
			UOpenMobileSensorDiscoveryLibrary::GetRequiredAccessForSensor(
				WorldContextObject,
				Sensor);
		Result.Permission = Access.AccessName;
		if (Access.Requirement == EOpenMobileSensorAccessRequirement::None)
		{
			Result.Status = EOpenMobilePermissionStatus::Granted;
			FinishSucceeded();
			return;
		}
		if (Access.Requirement ==
			EOpenMobileSensorAccessRequirement::ExternalPrerequisite)
		{
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NotSupported,
				TEXT("This sensor requires an external prerequisite. Supply it through the owning provider before retrying."));
			FinishFailed(Result.Error);
			return;
		}
		Permission = Access.Permission;
		Result.Permission =
			FOpenMobileSensorPermissions::GetPermissionName(Permission);
	}

	TWeakObjectPtr<UOpenMobileSensorPermissionAsyncAction> WeakThis(this);
	RequestHandle = GetSensorsSubsystem()->RequestPermissionNative(
		Permission,
		FOnOpenMobilePermissionRequestComplete::CreateLambda(
			[WeakThis](const FOpenMobilePermissionResult& InResult)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleComplete(InResult);
				}
			}
		)
	);
}

void UOpenMobileSensorPermissionAsyncAction::CancelNativeOperation()
{
	if (RequestHandle.IsValid() && GetSensorsSubsystem())
	{
		GetSensorsSubsystem()->CancelPermissionRequestNative(RequestHandle);
		RequestHandle.Reset();
	}
}

void UOpenMobileSensorPermissionAsyncAction::OnActionSucceeded()
{
	Completed.Broadcast(Result);
}

void UOpenMobileSensorPermissionAsyncAction::OnActionFailed(
	const FOpenMobileError& Error
)
{
	if (!Result.Error.IsSet())
	{
		Result.Error = Error;
	}
	Failed.Broadcast(Result);
}

void UOpenMobileSensorPermissionAsyncAction::OnActionCancelled(
	const FOpenMobileError& Error
)
{
	if (!Result.Error.IsSet())
	{
		Result.Error = Error;
	}
	Cancelled.Broadcast(Result);
}

void UOpenMobileSensorPermissionAsyncAction::HandleComplete(
	const FOpenMobilePermissionResult& InResult
)
{
	if (IsFinished())
	{
		return;
	}
	RequestHandle.Reset();
	Result = InResult;
	if (Result.Error.Code == EOpenMobileErrorCode::Cancelled)
	{
		FinishCancelled(Result.Error);
	}
	else if (Result.Error.IsSet())
	{
		FinishFailed(Result.Error);
	}
	else
	{
		FinishSucceeded();
	}
}

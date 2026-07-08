#include "OpenMobileNativeStepCountAsyncAction.h"

#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSubsystem.h"

UOpenMobileNativeStepCountAsyncAction*
UOpenMobileNativeStepCountAsyncAction::QueryNativeStepCount(
	const UObject* WorldContextObject,
	FOpenMobileNativeStepCountQuery Query
)
{
	UOpenMobileNativeStepCountAsyncAction* Action =
		NewObject<UOpenMobileNativeStepCountAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Query = Query;
	Action->Result.Query = Query;
	return Action;
}

void UOpenMobileNativeStepCountAsyncAction::Activate()
{
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}
	TWeakObjectPtr<UOpenMobileNativeStepCountAsyncAction> WeakThis(this);
	RequestId = GetSensorsSubsystem()->QueryNativeStepCountNative(
		Query,
		FOnOpenMobileNativeStepCountQueryComplete::CreateLambda(
			[WeakThis](const FOpenMobileNativeStepCountQueryResult& InResult)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleComplete(InResult);
				}
			}
		)
	);
}

void UOpenMobileNativeStepCountAsyncAction::CancelNativeOperation()
{
	if (RequestId.IsValid() && GetSensorsSubsystem())
	{
		GetSensorsSubsystem()->CancelNativeStepCountQueryNative(RequestId);
	}
}

void UOpenMobileNativeStepCountAsyncAction::OnActionSucceeded()
{
	Completed.Broadcast(Result);
}

void UOpenMobileNativeStepCountAsyncAction::OnActionFailed(
	const FOpenMobileError& Error
)
{
	if (!Result.Operation.Failure.IsSet())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::FromCommon(Error);
	}
	else if (!Result.Operation.Error.IsSet())
	{
		Result.Operation.Error = Error;
	}
	Failed.Broadcast(Result);
}

void UOpenMobileNativeStepCountAsyncAction::OnActionCancelled(
	const FOpenMobileError& Error
)
{
	Result.Operation = FOpenMobileSensorsErrorMapper::Map(
		EOpenMobileSensorFailureReason::Cancelled
	);
	Result.Operation.Error = Error;
	Cancelled.Broadcast(Result);
}

void UOpenMobileNativeStepCountAsyncAction::HandleComplete(
	const FOpenMobileNativeStepCountQueryResult& InResult
)
{
	if (IsFinished())
	{
		return;
	}
	Result = InResult;
	if (Result.Operation.Code == EOpenMobileSensorResultCode::Cancelled)
	{
		FinishCancelled(Result.Operation.Error);
	}
	else if (!Result.Operation.IsSuccess())
	{
		FinishFailed(Result.Operation.Error);
	}
	else
	{
		FinishSucceeded();
	}
}

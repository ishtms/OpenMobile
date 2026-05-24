#include "OpenMobileSensorFlushAsyncAction.h"

#include "OpenMobileSensorsSubsystem.h"

UOpenMobileSensorFlushAsyncAction*
UOpenMobileSensorFlushAsyncAction::FlushSensorSamples(
	const UObject* WorldContextObject,
	FOpenMobileSensorSubscriptionHandle Handle
)
{
	UOpenMobileSensorFlushAsyncAction* Action =
		NewObject<UOpenMobileSensorFlushAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Handle = Handle;
	Action->Result.Handle = Handle;
	return Action;
}

void UOpenMobileSensorFlushAsyncAction::Activate()
{
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}

	TWeakObjectPtr<UOpenMobileSensorFlushAsyncAction> WeakThis(this);
	GetSensorsSubsystem()->FlushNative(
		Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[WeakThis](const FOpenMobileSensorFlushResult& InResult)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleComplete(InResult);
				}
			}
		)
	);
}

void UOpenMobileSensorFlushAsyncAction::OnActionSucceeded()
{
	Completed.Broadcast(Result);
}

void UOpenMobileSensorFlushAsyncAction::OnActionFailed(
	const FOpenMobileError& Error
)
{
	if (!Result.Operation.Error.IsSet())
	{
		Result.Operation.Error = Error;
	}
	Failed.Broadcast(Result);
}

void UOpenMobileSensorFlushAsyncAction::OnActionCancelled(
	const FOpenMobileError& Error
)
{
	Result.Operation.Code = EOpenMobileSensorResultCode::Cancelled;
	Result.Operation.Error = Error;
	Cancelled.Broadcast(Result);
}

void UOpenMobileSensorFlushAsyncAction::HandleComplete(
	const FOpenMobileSensorFlushResult& InResult
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

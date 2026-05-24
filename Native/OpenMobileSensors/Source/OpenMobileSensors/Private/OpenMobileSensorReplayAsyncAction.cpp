#include "OpenMobileSensorReplayAsyncAction.h"

#include "OpenMobileSensorsSubsystem.h"

UOpenMobileSensorReplayAsyncAction*
UOpenMobileSensorReplayAsyncAction::ReplaySensorRecording(
	const UObject* WorldContextObject,
	FString FilePath,
	FOpenMobileSensorReplayOptions Options
)
{
	UOpenMobileSensorReplayAsyncAction* Action =
		NewObject<UOpenMobileSensorReplayAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->FilePath = MoveTemp(FilePath);
	Action->Options = MoveTemp(Options);
	return Action;
}

void UOpenMobileSensorReplayAsyncAction::Activate()
{
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}

	TWeakObjectPtr<UOpenMobileSensorReplayAsyncAction> WeakThis(this);
	GetSensorsSubsystem()->ReplayRecordingNative(
		FilePath,
		Options,
		FOnOpenMobileSensorReplayComplete::CreateLambda(
			[WeakThis](const FOpenMobileSensorReplayResult& InResult)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleComplete(InResult);
				}
			}
		)
	);
}

void UOpenMobileSensorReplayAsyncAction::OnActionSucceeded()
{
	Completed.Broadcast(Result);
}

void UOpenMobileSensorReplayAsyncAction::OnActionFailed(
	const FOpenMobileError& Error
)
{
	if (!Result.Operation.Error.IsSet())
	{
		Result.Operation.Error = Error;
	}
	Failed.Broadcast(Result);
}

void UOpenMobileSensorReplayAsyncAction::OnActionCancelled(
	const FOpenMobileError& Error
)
{
	Result.Operation.Code = EOpenMobileSensorResultCode::Cancelled;
	Result.Operation.Error = Error;
	Cancelled.Broadcast(Result);
}

void UOpenMobileSensorReplayAsyncAction::HandleComplete(
	const FOpenMobileSensorReplayResult& InResult
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

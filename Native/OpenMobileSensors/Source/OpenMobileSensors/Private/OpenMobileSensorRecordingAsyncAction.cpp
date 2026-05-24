#include "OpenMobileSensorRecordingAsyncAction.h"

#include "OpenMobileSensorsSubsystem.h"

UOpenMobileSensorRecordingAsyncAction*
UOpenMobileSensorRecordingAsyncAction::StartSensorRecording(
	const UObject* WorldContextObject,
	FOpenMobileSensorRecordingOptions Options
)
{
	UOpenMobileSensorRecordingAsyncAction* Action =
		NewObject<UOpenMobileSensorRecordingAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Operation = EOpenMobileSensorRecordingAsyncOperation::Start;
	Action->Options = MoveTemp(Options);
	return Action;
}

UOpenMobileSensorRecordingAsyncAction*
UOpenMobileSensorRecordingAsyncAction::StopSensorRecording(
	const UObject* WorldContextObject,
	FGuid RecordingRequestId
)
{
	UOpenMobileSensorRecordingAsyncAction* Action =
		NewObject<UOpenMobileSensorRecordingAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Operation = EOpenMobileSensorRecordingAsyncOperation::Stop;
	Action->RecordingRequestId = RecordingRequestId;
	Action->Result.Recording.RequestId = RecordingRequestId;
	return Action;
}

void UOpenMobileSensorRecordingAsyncAction::Activate()
{
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}

	TWeakObjectPtr<UOpenMobileSensorRecordingAsyncAction> WeakThis(this);
	FOnOpenMobileSensorRecordingComplete Completion =
		FOnOpenMobileSensorRecordingComplete::CreateLambda(
			[WeakThis](const FOpenMobileSensorRecordingResult& InResult)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleComplete(InResult);
				}
			}
		);
	if (Operation == EOpenMobileSensorRecordingAsyncOperation::Start)
	{
		RecordingRequestId = GetSensorsSubsystem()->StartRecordingNative(
			Options,
			MoveTemp(Completion)
		);
	}
	else
	{
		GetSensorsSubsystem()->StopRecordingNative(
			RecordingRequestId,
			MoveTemp(Completion)
		);
	}
}

void UOpenMobileSensorRecordingAsyncAction::OnActionSucceeded()
{
	Completed.Broadcast(Result);
}

void UOpenMobileSensorRecordingAsyncAction::OnActionFailed(
	const FOpenMobileError& Error
)
{
	if (!Result.Operation.Error.IsSet())
	{
		Result.Operation.Error = Error;
	}
	Failed.Broadcast(Result);
}

void UOpenMobileSensorRecordingAsyncAction::OnActionCancelled(
	const FOpenMobileError& Error
)
{
	Result.Operation.Code = EOpenMobileSensorResultCode::Cancelled;
	Result.Operation.Error = Error;
	Cancelled.Broadcast(Result);
}

void UOpenMobileSensorRecordingAsyncAction::HandleComplete(
	const FOpenMobileSensorRecordingResult& InResult
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

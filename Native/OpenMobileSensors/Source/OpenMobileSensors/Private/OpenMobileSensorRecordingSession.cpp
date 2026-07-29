#include "OpenMobileSensorRecordingSession.h"

#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsRecordingService.h"
#include "OpenMobileSensorsSubsystem.h"

UOpenMobileSensorRecordingSession*
UOpenMobileSensorRecordingSession::RecordSensors(
	const UObject* WorldContextObject,
	FOpenMobileSensorRecordingOptions Options,
	UObject* SessionOwner)
{
	UOpenMobileSensorRecordingSession* Session =
		NewObject<UOpenMobileSensorRecordingSession>();
	Session->ActivationWorldContext = const_cast<UObject*>(WorldContextObject);
	Session->LifetimeOwner = SessionOwner
		? SessionOwner
		: const_cast<UObject*>(WorldContextObject);
	Session->Options = MoveTemp(Options);
	return Session;
}

void UOpenMobileSensorRecordingSession::Activate()
{
	if (!InitializeAction(ActivationWorldContext))
	{
		return;
	}
	ActivationWorldContext = nullptr;
	TerminatedHandle =
		FOpenMobileSensorsRecordingService::OnRecordingTerminated().AddUObject(
			this,
			&UOpenMobileSensorRecordingSession::HandleTerminated);
	OwnerTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UOpenMobileSensorRecordingSession::TickOwner));
	Result.Recording.State = EOpenMobileSensorRecordingState::Starting;
	TWeakObjectPtr<UOpenMobileSensorRecordingSession> WeakThis(this);
	RequestId = GetSensorsSubsystem()->StartRecordingNative(
		Options,
		FOnOpenMobileSensorRecordingComplete::CreateLambda(
			[WeakThis](const FOpenMobileSensorRecordingResult& InResult)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleStart(InResult);
				}
			}));
}

void UOpenMobileSensorRecordingSession::FinalizeRecording()
{
	if (IsFinished() || bFinalizeRequested || !RequestId.IsValid())
	{
		return;
	}
	bFinalizeRequested = true;
	Result.Recording.State = EOpenMobileSensorRecordingState::Stopping;
	TWeakObjectPtr<UOpenMobileSensorRecordingSession> WeakThis(this);
	GetSensorsSubsystem()->StopRecordingNative(
		RequestId,
		FOnOpenMobileSensorRecordingComplete::CreateLambda(
			[WeakThis](const FOpenMobileSensorRecordingResult& InResult)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleFinalize(InResult);
				}
			}));
}

void UOpenMobileSensorRecordingSession::DiscardRecording()
{
	if (IsFinished())
	{
		return;
	}
	if (!GetSensorsSubsystem() || !RequestId.IsValid())
	{
		FinishCancelled(FOpenMobileError::Make(
			EOpenMobileErrorCode::Cancelled,
			TEXT("The sensor recording was discarded before it started.")));
		return;
	}
	const FOpenMobileSensorOperationResult Operation =
		GetSensorsSubsystem()->CancelRecordingNative(RequestId);
	if (IsFinished())
	{
		return;
	}
	if (!Operation.IsSuccess())
	{
		Result.Operation = Operation;
		FinishFailed(Operation.Error);
		return;
	}
	Result.Operation = FOpenMobileSensorsErrorMapper::Map(
		EOpenMobileSensorFailureReason::Cancelled);
	Result.Recording.State = EOpenMobileSensorRecordingState::Cancelled;
	FinishCancelled(Result.Operation.Error);
}

EOpenMobileSensorRecordingState
UOpenMobileSensorRecordingSession::GetRecordingState() const
{
	return Result.Recording.State;
}

FOpenMobileSensorRecordingSnapshot
UOpenMobileSensorRecordingSession::GetRecordingSnapshot() const
{
	return Result.Recording;
}

EOpenMobileSensorRecordingLimitReason
UOpenMobileSensorRecordingSession::GetLimitReason() const
{
	return LimitReason;
}

bool UOpenMobileSensorRecordingSession::IsActive() const
{
	return !IsFinished()
		&& (Result.Recording.State ==
				EOpenMobileSensorRecordingState::Starting
			|| Result.Recording.State ==
				EOpenMobileSensorRecordingState::Recording
			|| Result.Recording.State ==
				EOpenMobileSensorRecordingState::Stopping);
}

void UOpenMobileSensorRecordingSession::CancelNativeOperation()
{
	Unbind();
	if (GetSensorsSubsystem() && RequestId.IsValid())
	{
		GetSensorsSubsystem()->CancelRecordingNative(RequestId);
	}
	Result.Recording.State = EOpenMobileSensorRecordingState::Cancelled;
}

void UOpenMobileSensorRecordingSession::OnActionSucceeded()
{
	Unbind();
	Finalized.Broadcast(this, Result.Recording);
}

void UOpenMobileSensorRecordingSession::OnActionFailed(
	const FOpenMobileError& Error)
{
	Unbind();
	if (!Result.Operation.Failure.IsSet())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::FromCommon(Error);
	}
	else if (!Result.Operation.Error.IsSet())
	{
		Result.Operation.Error = Error;
	}
	const FString Message = !Result.Operation.Error.Message.IsEmpty()
		? Result.Operation.Error.Message
		: TEXT("The sensor recording failed.");
	Failed.Broadcast(
		this,
		FText::FromString(Message),
		FText::FromString(Result.Operation.Failure.Correction),
		Result);
}

void UOpenMobileSensorRecordingSession::OnActionCancelled(
	const FOpenMobileError& Error)
{
	static_cast<void>(Error);
	Unbind();
	Result.Recording.State = EOpenMobileSensorRecordingState::Cancelled;
	Cancelled.Broadcast(this, Result.Recording);
}

void UOpenMobileSensorRecordingSession::HandleStart(
	const FOpenMobileSensorRecordingResult& InResult)
{
	if (IsFinished())
	{
		return;
	}
	Result = InResult;
	RequestId = Result.Recording.RequestId;
	if (!Result.Operation.IsSuccess())
	{
		FinishFromResult(Result);
		return;
	}
	RecordingStarted.Broadcast(this, Result.Recording);
}

void UOpenMobileSensorRecordingSession::HandleFinalize(
	const FOpenMobileSensorRecordingResult& InResult)
{
	if (!IsFinished())
	{
		FinishFromResult(InResult);
	}
}

void UOpenMobileSensorRecordingSession::HandleTerminated(
	const FGuid& InRequestId,
	const FOpenMobileSensorRecordingResult& InResult,
	EOpenMobileSensorRecordingLimitReason InLimitReason)
{
	if (IsFinished() || InRequestId != RequestId)
	{
		return;
	}
	LimitReason = InLimitReason;
	Result = InResult;
	if (GetSensorsSubsystem())
	{
		GetSensorsSubsystem()->ReleaseRecordingSessionNative(RequestId);
	}
	if (LimitReason != EOpenMobileSensorRecordingLimitReason::None
		&& Result.Operation.IsSuccess())
	{
		LimitReached.Broadcast(this, LimitReason, Result.Recording);
	}
	FinishFromResult(Result);
}

bool UOpenMobileSensorRecordingSession::TickOwner(float DeltaSeconds)
{
	static_cast<void>(DeltaSeconds);
	if (IsFinished())
	{
		return false;
	}
	if (!LifetimeOwner.IsValid())
	{
		DiscardRecording();
		return false;
	}
	return true;
}

void UOpenMobileSensorRecordingSession::Unbind()
{
	if (TerminatedHandle.IsValid())
	{
		FOpenMobileSensorsRecordingService::OnRecordingTerminated().Remove(
			TerminatedHandle);
		TerminatedHandle.Reset();
	}
	if (OwnerTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(OwnerTickerHandle);
		OwnerTickerHandle.Reset();
	}
	LifetimeOwner.Reset();
}

void UOpenMobileSensorRecordingSession::FinishFromResult(
	const FOpenMobileSensorRecordingResult& InResult)
{
	Result = InResult;
	if (Result.Operation.Code == EOpenMobileSensorResultCode::Cancelled
		|| Result.Recording.State ==
			EOpenMobileSensorRecordingState::Cancelled)
	{
		FinishCancelled(Result.Operation.Error);
	}
	else if (!Result.Operation.IsSuccess()
		|| Result.Recording.State == EOpenMobileSensorRecordingState::Failed)
	{
		FinishFailed(Result.Operation.Error);
	}
	else if (Result.Recording.State ==
		EOpenMobileSensorRecordingState::Completed)
	{
		FinishSucceeded();
	}
}

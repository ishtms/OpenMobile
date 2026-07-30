#include "OpenMobileSensorReplaySession.h"

#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSubsystem.h"

UOpenMobileSensorReplaySession*
UOpenMobileSensorReplaySession::ReplaySensorFile(
	const UObject* WorldContextObject,
	FString FilePath,
	FOpenMobileSensorReplayOptions Options,
	UObject* SessionOwner)
{
	UOpenMobileSensorReplaySession* Session =
		NewObject<UOpenMobileSensorReplaySession>();
	Session->ActivationWorldContext = const_cast<UObject*>(WorldContextObject);
	Session->LifetimeOwner = SessionOwner
		? SessionOwner
		: const_cast<UObject*>(WorldContextObject);
	Session->FilePath = MoveTemp(FilePath);
	Session->Options = Options;
	return Session;
}

void UOpenMobileSensorReplaySession::Activate()
{
	if (!InitializeAction(ActivationWorldContext))
	{
		return;
	}
	ActivationWorldContext = nullptr;
	Snapshot.State = EOpenMobileSensorReplayState::Loading;
	Snapshot.PlaybackSpeed = Options.PlaybackSpeed;
	Snapshot.bLoop = Options.bLoop;
	Snapshot.ClockMode = Options.ClockMode;
	SessionTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UOpenMobileSensorReplaySession::TickSession));
	TWeakObjectPtr<UOpenMobileSensorReplaySession> WeakThis(this);
	RequestId = GetSensorsSubsystem()->ReplayRecordingNative(
		FilePath,
		Options,
		FOnOpenMobileSensorReplayComplete::CreateLambda(
			[WeakThis](const FOpenMobileSensorReplayResult& InResult)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleComplete(InResult);
				}
			}));
	Snapshot.RequestId = RequestId;
}

FOpenMobileSensorOperationResult UOpenMobileSensorReplaySession::PauseReplay()
{
	if (!GetSensorsSubsystem() || !RequestId.IsValid() || IsFinished())
	{
		return InvalidSessionResult();
	}
	const FOpenMobileSensorOperationResult Operation =
		GetSensorsSubsystem()->PauseReplayNative(RequestId);
	if (Operation.IsSuccess())
	{
		RefreshSnapshot(true);
	}
	return Operation;
}

FOpenMobileSensorOperationResult UOpenMobileSensorReplaySession::ResumeReplay()
{
	if (!GetSensorsSubsystem() || !RequestId.IsValid() || IsFinished())
	{
		return InvalidSessionResult();
	}
	const FOpenMobileSensorOperationResult Operation =
		GetSensorsSubsystem()->ResumeReplayNative(RequestId);
	if (Operation.IsSuccess())
	{
		RefreshSnapshot(true);
	}
	return Operation;
}

FOpenMobileSensorOperationResult UOpenMobileSensorReplaySession::SeekReplayTo(
	FTimespan PlaybackPosition)
{
	if (!GetSensorsSubsystem() || !RequestId.IsValid() || IsFinished())
	{
		return InvalidSessionResult();
	}
	const FOpenMobileSensorOperationResult Operation =
		GetSensorsSubsystem()->SeekReplayNative(
			RequestId, PlaybackPosition.GetTotalSeconds());
	if (Operation.IsSuccess())
	{
		RefreshSnapshot(false);
	}
	return Operation;
}

FOpenMobileSensorOperationResult UOpenMobileSensorReplaySession::
SetReplaySpeed(double PlaybackSpeed)
{
	if (!GetSensorsSubsystem() || !RequestId.IsValid() || IsFinished())
	{
		return InvalidSessionResult();
	}
	const FOpenMobileSensorOperationResult Operation =
		GetSensorsSubsystem()->SetReplaySpeedNative(RequestId, PlaybackSpeed);
	if (Operation.IsSuccess())
	{
		RefreshSnapshot(false);
	}
	return Operation;
}

FOpenMobileSensorOperationResult UOpenMobileSensorReplaySession::
SetReplayLooping(bool bLoop)
{
	if (!GetSensorsSubsystem() || !RequestId.IsValid() || IsFinished())
	{
		return InvalidSessionResult();
	}
	const FOpenMobileSensorOperationResult Operation =
		GetSensorsSubsystem()->SetReplayLoopingNative(RequestId, bLoop);
	if (Operation.IsSuccess())
	{
		RefreshSnapshot(false);
	}
	return Operation;
}

FOpenMobileSensorOperationResult UOpenMobileSensorReplaySession::
AdvanceReplayBy(FTimespan Delta)
{
	if (!GetSensorsSubsystem() || !RequestId.IsValid() || IsFinished())
	{
		return InvalidSessionResult();
	}
	const FOpenMobileSensorOperationResult Operation =
		GetSensorsSubsystem()->AdvanceReplayNative(
			RequestId, Delta.GetTotalSeconds());
	if (Operation.IsSuccess() && !IsFinished())
	{
		RefreshSnapshot(true);
	}
	return Operation;
}

FOpenMobileSensorOperationResult UOpenMobileSensorReplaySession::StopReplay()
{
	if (IsFinished())
	{
		FOpenMobileSensorOperationResult Operation;
		Operation.Code = EOpenMobileSensorResultCode::Success;
		return Operation;
	}
	if (!GetSensorsSubsystem() || !RequestId.IsValid())
	{
		return InvalidSessionResult();
	}
	const FOpenMobileSensorOperationResult Operation =
		GetSensorsSubsystem()->CancelReplayNative(RequestId);
	if (Operation.IsSuccess() && !IsFinished())
	{
		Snapshot.State = EOpenMobileSensorReplayState::Cancelled;
		FinishCancelled(FOpenMobileError::Make(
			EOpenMobileErrorCode::Cancelled,
			TEXT("The sensor replay was stopped.")));
	}
	return Operation;
}

EOpenMobileSensorReplayState
UOpenMobileSensorReplaySession::GetReplayState() const
{
	return Snapshot.State;
}

FOpenMobileSensorReplaySnapshot
UOpenMobileSensorReplaySession::GetReplaySnapshot() const
{
	return Snapshot;
}

FTimespan UOpenMobileSensorReplaySession::GetReplayPosition() const
{
	return FTimespan::FromSeconds(Snapshot.PlaybackTimeSeconds);
}

FTimespan UOpenMobileSensorReplaySession::GetReplayDuration() const
{
	return FTimespan::FromSeconds(Snapshot.DurationSeconds);
}

bool UOpenMobileSensorReplaySession::IsActive() const
{
	return !IsFinished()
		&& (Snapshot.State == EOpenMobileSensorReplayState::Loading
			|| Snapshot.State == EOpenMobileSensorReplayState::Playing
			|| Snapshot.State == EOpenMobileSensorReplayState::Paused);
}

#if WITH_DEV_AUTOMATION_TESTS
bool UOpenMobileSensorReplaySession::TickForTests()
{
	return TickSession(0.0f);
}
#endif

void UOpenMobileSensorReplaySession::CancelNativeOperation()
{
	Unbind();
	if (GetSensorsSubsystem() && RequestId.IsValid())
	{
		GetSensorsSubsystem()->CancelReplayNative(RequestId);
	}
	Snapshot.State = EOpenMobileSensorReplayState::Cancelled;
}

void UOpenMobileSensorReplaySession::OnActionSucceeded()
{
	Unbind();
	Completed.Broadcast(this, Result);
}

void UOpenMobileSensorReplaySession::OnActionFailed(
	const FOpenMobileError& Error)
{
	Unbind();
	if (Snapshot.State != EOpenMobileSensorReplayState::Failed)
	{
		Snapshot.State = EOpenMobileSensorReplayState::Failed;
		StateChanged.Broadcast(this, Snapshot);
	}
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
		: TEXT("The sensor replay failed.");
	Failed.Broadcast(
		this,
		FText::FromString(Message),
		FText::FromString(Result.Operation.Failure.Correction),
		Result);
}

void UOpenMobileSensorReplaySession::OnActionCancelled(
	const FOpenMobileError& Error)
{
	static_cast<void>(Error);
	Unbind();
	if (Snapshot.State != EOpenMobileSensorReplayState::Cancelled)
	{
		Snapshot.State = EOpenMobileSensorReplayState::Cancelled;
		StateChanged.Broadcast(this, Snapshot);
	}
	Cancelled.Broadcast(this, Result);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorReplaySession::InvalidSessionResult() const
{
	return FOpenMobileSensorsErrorMapper::Map(
		EOpenMobileSensorFailureReason::InvalidHandle);
}

void UOpenMobileSensorReplaySession::HandleComplete(
	const FOpenMobileSensorReplayResult& InResult)
{
	if (IsFinished())
	{
		return;
	}
	Result = InResult;
	RequestId = Result.RequestId;
	Snapshot.RequestId = Result.RequestId;
	Snapshot.PlaybackTimeSeconds = Result.PlaybackTimeSeconds;
	if (Result.Operation.Code == EOpenMobileSensorResultCode::Cancelled)
	{
		Snapshot.State = EOpenMobileSensorReplayState::Cancelled;
		StateChanged.Broadcast(this, Snapshot);
		FinishCancelled(Result.Operation.Error);
	}
	else if (!Result.Operation.IsSuccess())
	{
		Snapshot.State = EOpenMobileSensorReplayState::Failed;
		StateChanged.Broadcast(this, Snapshot);
		FinishFailed(Result.Operation.Error);
	}
	else
	{
		Snapshot.State = EOpenMobileSensorReplayState::Completed;
		StateChanged.Broadcast(this, Snapshot);
		FinishSucceeded();
	}
}

bool UOpenMobileSensorReplaySession::TickSession(float DeltaSeconds)
{
	static_cast<void>(DeltaSeconds);
	if (IsFinished())
	{
		return false;
	}
	if (!LifetimeOwner.IsValid())
	{
		StopReplay();
		return false;
	}
	RefreshSnapshot(true);
	return !IsFinished();
}

void UOpenMobileSensorReplaySession::RefreshSnapshot(
	bool bBroadcastTransitions)
{
	if (!GetSensorsSubsystem() || !RequestId.IsValid() || IsFinished())
	{
		return;
	}
	FOpenMobileSensorReplaySnapshot Updated;
	if (!GetSensorsSubsystem()->GetReplayStateNative(RequestId, Updated))
	{
		return;
	}
	const EOpenMobileSensorReplayState Previous = Snapshot.State;
	const double PreviousPlaybackTimeSeconds = Snapshot.PlaybackTimeSeconds;
	Snapshot = Updated;
	if (!bBroadcastTransitions)
	{
		return;
	}
	if (!bStartedBroadcast
		&& (Snapshot.State == EOpenMobileSensorReplayState::Playing
			|| Snapshot.State == EOpenMobileSensorReplayState::Paused))
	{
		bStartedBroadcast = true;
		ReplayStarted.Broadcast(this, Snapshot);
		StateChanged.Broadcast(this, Snapshot);
		return;
	}
	if (Snapshot.bLoop
		&& Snapshot.State == EOpenMobileSensorReplayState::Playing
		&& Snapshot.PlaybackTimeSeconds < PreviousPlaybackTimeSeconds)
	{
		Looped.Broadcast(this, Snapshot);
	}
	if (Previous != Snapshot.State)
	{
		StateChanged.Broadcast(this, Snapshot);
		if (Snapshot.State == EOpenMobileSensorReplayState::Paused)
		{
			Paused.Broadcast(this, Snapshot);
		}
		else if (Previous == EOpenMobileSensorReplayState::Paused
			&& Snapshot.State == EOpenMobileSensorReplayState::Playing)
		{
			Resumed.Broadcast(this, Snapshot);
		}
	}
}

void UOpenMobileSensorReplaySession::Unbind()
{
	if (SessionTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(SessionTickerHandle);
		SessionTickerHandle.Reset();
	}
	LifetimeOwner.Reset();
}

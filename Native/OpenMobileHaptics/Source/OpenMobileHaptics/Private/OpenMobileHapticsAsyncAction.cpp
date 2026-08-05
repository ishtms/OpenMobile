#include "OpenMobileHapticsAsyncAction.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileHapticsErrorMapper.h"
#include "OpenMobileHapticsSubsystem.h"

namespace OpenMobileHapticsAsyncActionPrivate
{
	FOpenMobileHapticError MakeError(
		EOpenMobileHapticsFailureReason Reason,
		EOpenMobileHapticFailureStage Stage,
		FName FailedItem = NAME_None,
		FOpenMobileHapticPlaybackHandle Handle = {},
		bool bAfterAcceptance = false
	)
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = Reason;
		Context.Stage = Stage;
		Context.FailedItem = FailedItem;
		Context.Handle = Handle;
		Context.bAfterAcceptance = bAfterAcceptance;
		return FOpenMobileHapticsErrorMapper::Map(Context);
	}
}

UOpenMobileHapticPlaybackAsyncAction*
UOpenMobileHapticPlaybackAsyncAction::PlayNamedHapticAsync(
	const UObject* WorldContextObject,
	FName PatternName,
	float Intensity,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	UOpenMobileHapticPlaybackAsyncAction* Action =
		NewObject<UOpenMobileHapticPlaybackAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->RequestedPatternName = PatternName;
	Action->RequestedIntensity = Intensity;
	Action->RequestedOptions = Options;
	return Action;
}

void UOpenMobileHapticPlaybackAsyncAction::Activate()
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return;
	}
	if (!StoredWorldContextObject || !GEngine)
	{
		FinishFailed(FOpenMobileHapticPlaybackResult::MakeRejected(
			OpenMobileHapticsAsyncActionPrivate::MakeError(
				EOpenMobileHapticsFailureReason::InvalidRequest,
				EOpenMobileHapticFailureStage::Validation,
				RequestedPatternName
			)
		));
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(
		StoredWorldContextObject,
		EGetWorldErrorMode::ReturnNull
	);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!World || !GameInstance)
	{
		FinishFailed(FOpenMobileHapticPlaybackResult::MakeRejected(
			OpenMobileHapticsAsyncActionPrivate::MakeError(
				EOpenMobileHapticsFailureReason::LifecycleRestricted,
				EOpenMobileHapticFailureStage::Lifecycle,
				RequestedPatternName
			)
		));
		return;
	}

	RegisterWithGameInstance(StoredWorldContextObject);
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileHapticPlaybackAsyncAction::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishFailed(FOpenMobileHapticPlaybackResult::MakeRejected(
			OpenMobileHapticsAsyncActionPrivate::MakeError(
				EOpenMobileHapticsFailureReason::BackendUnavailable,
				EOpenMobileHapticFailureStage::Lifecycle,
				RequestedPatternName
			)
		));
		return;
	}
	Subsystem->RegisterAsyncAction(this);

	FOpenMobileHapticNamedPatternRequest Request;
	Request.PatternName = RequestedPatternName;
	Request.Intensity = RequestedIntensity;
	Request.Options = RequestedOptions;
	ImmediateResult = Subsystem->SubmitNamedPattern(Request);
	PlaybackHandle = ImmediateResult.Handle;
	if (ImmediateResult.Outcome == EOpenMobileHapticPlaybackOutcome::Suppressed)
	{
		FinishSuppressed(ImmediateResult);
		return;
	}
	if (!ImmediateResult.IsAccepted())
	{
		FinishFailed(ImmediateResult);
		return;
	}
	if (!PlaybackHandle.IsValid())
	{
		FinishFailed(FOpenMobileHapticPlaybackResult::MakeRejected(
			OpenMobileHapticsAsyncActionPrivate::MakeError(
				EOpenMobileHapticsFailureReason::Internal,
				EOpenMobileHapticFailureStage::NativeSubmission,
				RequestedPatternName,
				{},
				true
			)
		));
		return;
	}

	PlaybackEventHandle = Subsystem->OnPlaybackEventNative().AddUObject(
		this,
		&UOpenMobileHapticPlaybackAsyncAction::HandlePlaybackEvent
	);
	Accepted.Broadcast(ImmediateResult);
}

void UOpenMobileHapticPlaybackAsyncAction::Cancel()
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return;
	}
	if (Subsystem.IsValid() && PlaybackHandle.IsValid())
	{
		Subsystem->CancelPlaybackNative(PlaybackHandle);
	}

	FOpenMobileHapticPlaybackResult Result = ImmediateResult;
	Result.Handle = PlaybackHandle;
	Result.State = EOpenMobileHapticPlaybackState::Cancelled;
	Result.Error = OpenMobileHapticsAsyncActionPrivate::MakeError(
		EOpenMobileHapticsFailureReason::Cancelled,
		EOpenMobileHapticFailureStage::Playback,
		RequestedPatternName,
		PlaybackHandle,
		PlaybackHandle.IsValid()
	);
	FinishCancelled(MoveTemp(Result));
}

bool UOpenMobileHapticPlaybackAsyncAction::TrySetTerminalState(
	EOpenMobileHapticAsyncTerminalState State
)
{
	if (TerminalState != EOpenMobileHapticAsyncTerminalState::Pending)
	{
		return false;
	}
	TerminalState = State;
	return true;
}

void UOpenMobileHapticPlaybackAsyncAction::FinishCompleted(
	FOpenMobileHapticPlaybackResult Result
)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileHapticAsyncTerminalState::Completed))
	{
		return;
	}
	Cleanup();
	Completed.Broadcast(Result);
	NativeTerminal.Broadcast(
		EOpenMobileHapticAsyncTerminalState::Completed,
		Result
	);
	SetReadyToDestroy();
}

void UOpenMobileHapticPlaybackAsyncAction::FinishStopped(
	FOpenMobileHapticPlaybackResult Result
)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileHapticAsyncTerminalState::Stopped))
	{
		return;
	}
	Cleanup();
	Stopped.Broadcast(Result);
	NativeTerminal.Broadcast(EOpenMobileHapticAsyncTerminalState::Stopped, Result);
	SetReadyToDestroy();
}

void UOpenMobileHapticPlaybackAsyncAction::FinishCancelled(
	FOpenMobileHapticPlaybackResult Result
)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileHapticAsyncTerminalState::Cancelled))
	{
		return;
	}
	Cleanup();
	Cancelled.Broadcast(Result);
	NativeTerminal.Broadcast(
		EOpenMobileHapticAsyncTerminalState::Cancelled,
		Result
	);
	SetReadyToDestroy();
}

void UOpenMobileHapticPlaybackAsyncAction::FinishSuppressed(
	FOpenMobileHapticPlaybackResult Result
)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileHapticAsyncTerminalState::Suppressed))
	{
		return;
	}
	Cleanup();
	Suppressed.Broadcast(Result);
	NativeTerminal.Broadcast(
		EOpenMobileHapticAsyncTerminalState::Suppressed,
		Result
	);
	SetReadyToDestroy();
}

void UOpenMobileHapticPlaybackAsyncAction::FinishInterrupted(
	FOpenMobileHapticPlaybackResult Result
)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileHapticAsyncTerminalState::Interrupted))
	{
		return;
	}
	Cleanup();
	Interrupted.Broadcast(Result);
	NativeTerminal.Broadcast(
		EOpenMobileHapticAsyncTerminalState::Interrupted,
		Result
	);
	SetReadyToDestroy();
}

void UOpenMobileHapticPlaybackAsyncAction::FinishFailed(
	FOpenMobileHapticPlaybackResult Result
)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileHapticAsyncTerminalState::Failed))
	{
		return;
	}
	Cleanup();
	Failed.Broadcast(Result);
	NativeTerminal.Broadcast(EOpenMobileHapticAsyncTerminalState::Failed, Result);
	SetReadyToDestroy();
}

void UOpenMobileHapticPlaybackAsyncAction::HandlePlaybackEvent(
	const FOpenMobileHapticPlaybackEvent& Event
)
{
	check(IsInGameThread());
	if (IsFinished() || Event.Handle != PlaybackHandle)
	{
		return;
	}

	FOpenMobileHapticPlaybackResult Result = ImmediateResult;
	Result.Handle = Event.Handle;
	Result.State = Event.State;
	Result.Channel = Event.Channel;
	Result.ResolvedPath = Event.ResolvedPath;
	Result.Error = Event.Error;
	switch (Event.State)
	{
	case EOpenMobileHapticPlaybackState::Started:
		Started.Broadcast(Result);
		break;
	case EOpenMobileHapticPlaybackState::Completed:
		FinishCompleted(MoveTemp(Result));
		break;
	case EOpenMobileHapticPlaybackState::Cancelled:
		if (!Result.Error.IsSet())
		{
			Result.Error = OpenMobileHapticsAsyncActionPrivate::MakeError(
				EOpenMobileHapticsFailureReason::Cancelled,
				EOpenMobileHapticFailureStage::Playback,
				RequestedPatternName,
				PlaybackHandle,
				true
			);
		}
		FinishCancelled(MoveTemp(Result));
		break;
	case EOpenMobileHapticPlaybackState::Stopped:
		FinishStopped(MoveTemp(Result));
		break;
	case EOpenMobileHapticPlaybackState::Interrupted:
		if (!Result.Error.IsSet())
		{
			Result.Error = OpenMobileHapticsAsyncActionPrivate::MakeError(
				EOpenMobileHapticsFailureReason::Interrupted,
				EOpenMobileHapticFailureStage::Interruption,
				RequestedPatternName,
				PlaybackHandle,
				true
			);
		}
		FinishInterrupted(MoveTemp(Result));
		break;
	case EOpenMobileHapticPlaybackState::Failed:
		if (!Result.Error.IsSet())
		{
			Result.Error = OpenMobileHapticsAsyncActionPrivate::MakeError(
				EOpenMobileHapticsFailureReason::NativeEngineFailure,
				EOpenMobileHapticFailureStage::Playback,
				RequestedPatternName,
				PlaybackHandle,
				true
			);
		}
		FinishFailed(MoveTemp(Result));
		break;
	default:
		break;
	}
}

void UOpenMobileHapticPlaybackAsyncAction::HandleWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources
)
{
	static_cast<void>(bSessionEnded);
	static_cast<void>(bCleanupResources);
	if (IsFinished() || World != TargetWorld.Get())
	{
		return;
	}

	FOpenMobileHapticPlaybackResult Result = ImmediateResult;
	Result.Handle = PlaybackHandle;
	Result.State = EOpenMobileHapticPlaybackState::Cancelled;
	Result.Error = OpenMobileHapticsAsyncActionPrivate::MakeError(
		EOpenMobileHapticsFailureReason::Cancelled,
		EOpenMobileHapticFailureStage::Shutdown,
		RequestedPatternName,
		PlaybackHandle,
		PlaybackHandle.IsValid()
	);
	FinishCancelled(MoveTemp(Result));
}

void UOpenMobileHapticPlaybackAsyncAction::HandleGameInstanceTeardown()
{
	if (IsFinished())
	{
		return;
	}
	FOpenMobileHapticPlaybackResult Result = ImmediateResult;
	Result.Handle = PlaybackHandle;
	Result.State = EOpenMobileHapticPlaybackState::Cancelled;
	Result.Error = OpenMobileHapticsAsyncActionPrivate::MakeError(
		EOpenMobileHapticsFailureReason::Cancelled,
		EOpenMobileHapticFailureStage::Shutdown,
		RequestedPatternName,
		PlaybackHandle,
		PlaybackHandle.IsValid()
	);
	FinishCancelled(MoveTemp(Result));
}

void UOpenMobileHapticPlaybackAsyncAction::Cleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
	if (Subsystem.IsValid())
	{
		if (PlaybackEventHandle.IsValid())
		{
			Subsystem->OnPlaybackEventNative().Remove(PlaybackEventHandle);
			PlaybackEventHandle.Reset();
		}
		Subsystem->UnregisterAsyncAction(this);
		Subsystem.Reset();
	}
	TargetWorld.Reset();
	StoredWorldContextObject = nullptr;
}

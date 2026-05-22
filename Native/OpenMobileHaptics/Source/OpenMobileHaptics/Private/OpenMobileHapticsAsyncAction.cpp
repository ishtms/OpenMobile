#include "OpenMobileHapticsAsyncAction.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileHapticsSubsystem.h"

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
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The Haptics async action requires a valid world context object.")
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
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Haptics async action could not resolve a Game Instance.")
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
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Open Mobile Haptics subsystem is unavailable.")
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
		ImmediateResult.State = EOpenMobileHapticPlaybackState::Completed;
		FinishCompleted(ImmediateResult);
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
			EOpenMobileErrorCode::Internal,
			TEXT("Accepted asynchronous Haptics playback returned no handle.")
		));
		return;
	}

	PlaybackEventHandle = Subsystem->OnPlaybackEventNative().AddUObject(
		this,
		&UOpenMobileHapticPlaybackAsyncAction::HandlePlaybackEvent
	);
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
		Subsystem->StopPlaybackNative(PlaybackHandle);
	}

	FOpenMobileHapticPlaybackResult Result = ImmediateResult;
	Result.Handle = PlaybackHandle;
	Result.State = EOpenMobileHapticPlaybackState::Cancelled;
	Result.Error = FOpenMobileError::Make(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The Haptics async action was cancelled.")
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
	case EOpenMobileHapticPlaybackState::Completed:
		FinishCompleted(MoveTemp(Result));
		break;
	case EOpenMobileHapticPlaybackState::Stopped:
	case EOpenMobileHapticPlaybackState::Cancelled:
		if (!Result.Error.IsSet())
		{
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Cancelled,
				TEXT("Haptics playback ended before completion.")
			);
		}
		FinishCancelled(MoveTemp(Result));
		break;
	case EOpenMobileHapticPlaybackState::Interrupted:
	case EOpenMobileHapticPlaybackState::Failed:
		if (!Result.Error.IsSet())
		{
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("Haptics playback failed after acceptance.")
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
	Result.Error = FOpenMobileError::Make(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The Haptics async action was cancelled because its world is shutting down.")
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
	Result.Error = FOpenMobileError::Make(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The Haptics async action was cancelled because its Game Instance is shutting down.")
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

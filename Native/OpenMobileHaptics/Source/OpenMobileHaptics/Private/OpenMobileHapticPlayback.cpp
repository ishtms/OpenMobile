#include "OpenMobileHapticPlayback.h"

#include "Async/Async.h"
#include "OpenMobileHapticPreparationLease.h"
#include "OpenMobileHapticsSubsystem.h"

namespace OpenMobileHapticPlaybackPrivate
{
	/** Treats queued, playing, and paused handles as live so Blueprint objects don't finish before admission or resume. */
	bool IsActiveState(EOpenMobileHapticPlaybackState State)
	{
		switch (State)
		{
		case EOpenMobileHapticPlaybackState::Accepted:
		case EOpenMobileHapticPlaybackState::Scheduled:
		case EOpenMobileHapticPlaybackState::Started:
		case EOpenMobileHapticPlaybackState::Paused:
		case EOpenMobileHapticPlaybackState::Resumed:
			return true;
		default:
			return false;
		}
	}

	/** Maps subsystem control outcomes to the compact Blueprint branch enum without losing rejection versus unsupported. */
	EOpenMobileHapticControlBranch ToBranch(
		EOpenMobileHapticControlOutcome Outcome
	)
	{
		switch (Outcome)
		{
		case EOpenMobileHapticControlOutcome::Accepted:
			return EOpenMobileHapticControlBranch::Succeeded;
		case EOpenMobileHapticControlOutcome::Unsupported:
			return EOpenMobileHapticControlBranch::Unsupported;
		case EOpenMobileHapticControlOutcome::StaleHandle:
			return EOpenMobileHapticControlBranch::Stale;
		default:
			return EOpenMobileHapticControlBranch::Failed;
		}
	}
}

bool UOpenMobileHapticPlayback::IsValid() const
{
	return !bFinished && Handle.IsValid() && Subsystem.IsValid();
}

bool UOpenMobileHapticPlayback::IsActive() const
{
	return IsValid() && OpenMobileHapticPlaybackPrivate::IsActiveState(State);
}

bool UOpenMobileHapticPlayback::IsPaused() const
{
	return IsValid() && State == EOpenMobileHapticPlaybackState::Paused;
}

void UOpenMobileHapticPlayback::Stop(
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	const FOpenMobileHapticControlResult Result = IsValid()
		? Subsystem->StopPlaybackNative(Handle)
		: MakeStaleControlResult();
	ResolveControlResult(Result, Outcome, Error);
}

void UOpenMobileHapticPlayback::Cancel(
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	const FOpenMobileHapticControlResult Result = IsValid()
		? Subsystem->CancelPlaybackNative(Handle)
		: MakeStaleControlResult();
	ResolveControlResult(Result, Outcome, Error);
}

void UOpenMobileHapticPlayback::Pause(
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	const FOpenMobileHapticControlResult Result = IsValid()
		? Subsystem->PausePlaybackNative(Handle)
		: MakeStaleControlResult();
	ResolveControlResult(Result, Outcome, Error);
}

void UOpenMobileHapticPlayback::Resume(
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	const FOpenMobileHapticControlResult Result = IsValid()
		? Subsystem->ResumePlaybackNative(Handle)
		: MakeStaleControlResult();
	ResolveControlResult(Result, Outcome, Error);
}

void UOpenMobileHapticPlayback::Seek(
	double PositionSeconds,
	EOpenMobileHapticControlBranch& Outcome,
	double& ResolvedPositionSeconds,
	bool& bWasQuantized,
	FOpenMobileHapticError& Error
)
{
	const FOpenMobileHapticControlResult Result = IsValid()
		? Subsystem->SeekPlaybackNative(
			Handle,
			FMath::Max(0.0, PositionSeconds)
		)
		: MakeStaleControlResult();
	ResolvedPositionSeconds = Result.ResolvedPositionSeconds;
	bWasQuantized = Result.bQuantized;
	ResolveControlResult(Result, Outcome, Error);
}

void UOpenMobileHapticPlayback::SetIntensity(
	float Intensity,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	FOpenMobileHapticDynamicParameterUpdate Update;
	Update.bUpdateIntensity = true;
	Update.Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	Update.bUpdateSharpness = false;
	const FOpenMobileHapticControlResult Result = IsValid()
		? Subsystem->UpdatePlaybackParametersNative(Handle, Update)
		: MakeStaleControlResult();
	ResolveControlResult(Result, Outcome, Error);
}

void UOpenMobileHapticPlayback::SetSharpness(
	float Sharpness,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	FOpenMobileHapticDynamicParameterUpdate Update;
	Update.bUpdateIntensity = false;
	Update.bUpdateSharpness = true;
	Update.Sharpness = FMath::Clamp(Sharpness, 0.0f, 1.0f);
	const FOpenMobileHapticControlResult Result = IsValid()
		? Subsystem->UpdatePlaybackParametersNative(Handle, Update)
		: MakeStaleControlResult();
	ResolveControlResult(Result, Outcome, Error);
}

void UOpenMobileHapticPlayback::SetIntensityAndSharpness(
	float Intensity,
	float Sharpness,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	FOpenMobileHapticDynamicParameterUpdate Update;
	Update.bUpdateIntensity = true;
	Update.Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	Update.bUpdateSharpness = true;
	Update.Sharpness = FMath::Clamp(Sharpness, 0.0f, 1.0f);
	const FOpenMobileHapticControlResult Result = IsValid()
		? Subsystem->UpdatePlaybackParametersNative(Handle, Update)
		: MakeStaleControlResult();
	ResolveControlResult(Result, Outcome, Error);
}

void UOpenMobileHapticPlayback::BeginDestroy()
{
	Cleanup();
	Super::BeginDestroy();
}

void UOpenMobileHapticPlayback::InitializePlayback(
	UOpenMobileHapticsSubsystem* InSubsystem,
	const FOpenMobileHapticPlaybackResult& Result,
	FName InPatternOrEffect
)
{
	Subsystem = InSubsystem;
	Handle = Result.Handle;
	ImmediateResult = Result;
	State = Result.State;
	PatternOrEffect = InPatternOrEffect;
	Channel = Result.Channel;
	bUsedFallback =
		Result.Outcome == EOpenMobileHapticPlaybackOutcome::Fallback;
	ResolvedQuality = Result.ResolvedPath;
	PlaybackEventHandle = InSubsystem->OnPlaybackEventNative().AddUObject(
		this,
		&UOpenMobileHapticPlayback::HandlePlaybackEvent
	);
	InSubsystem->RegisterPlaybackObject(this);

	TWeakObjectPtr<UOpenMobileHapticPlayback> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->BroadcastAcceptedIfNeeded();
		}
	});
}

void UOpenMobileHapticPlayback::HandlePlaybackEvent(
	const FOpenMobileHapticPlaybackEvent& Event
)
{
	if (bFinished || Event.Handle != Handle)
	{
		return;
	}
	BroadcastAcceptedIfNeeded();
	State = Event.State;
	Channel = Event.Channel;
	ResolvedQuality = Event.ResolvedPath;
	switch (Event.State)
	{
	case EOpenMobileHapticPlaybackState::Started:
		OnStarted.Broadcast(Event.Evidence);
		break;
	case EOpenMobileHapticPlaybackState::Completed:
		Finish(EOpenMobileHapticTerminalReason::Completed, Event.Error);
		break;
	case EOpenMobileHapticPlaybackState::Stopped:
		Finish(EOpenMobileHapticTerminalReason::Stopped, Event.Error);
		break;
	case EOpenMobileHapticPlaybackState::Cancelled:
		Finish(EOpenMobileHapticTerminalReason::Cancelled, Event.Error);
		break;
	case EOpenMobileHapticPlaybackState::Interrupted:
		Finish(EOpenMobileHapticTerminalReason::Interrupted, Event.Error);
		break;
	case EOpenMobileHapticPlaybackState::Failed:
		Finish(EOpenMobileHapticTerminalReason::Failed, Event.Error);
		break;
	default:
		break;
	}
}

void UOpenMobileHapticPlayback::AttachPreparationLease(
	UOpenMobileHapticPreparationLease* InPreparationLease
)
{
	PreparationLease = InPreparationLease;
}

void UOpenMobileHapticPlayback::HandleGameInstanceTeardown()
{
	if (!bFinished)
	{
		State = EOpenMobileHapticPlaybackState::Cancelled;
		Finish(
			EOpenMobileHapticTerminalReason::Cancelled,
			FOpenMobileHapticError::FromCommon(
				EOpenMobileErrorCode::Cancelled,
				TEXT("The owning Game Instance ended."),
				EOpenMobileHapticFailureStage::Shutdown
			)
		);
	}
}

void UOpenMobileHapticPlayback::BroadcastAcceptedIfNeeded()
{
	if (!bAcceptedBroadcast)
	{
		bAcceptedBroadcast = true;
		OnAccepted.Broadcast(ImmediateResult);
	}
}

void UOpenMobileHapticPlayback::Finish(
	EOpenMobileHapticTerminalReason Reason,
	const FOpenMobileHapticError& Error
)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	TerminalReason = Reason;
	Cleanup();
	OnFinished.Broadcast(Reason, Error);
}

void UOpenMobileHapticPlayback::Cleanup()
{
	if (Subsystem.IsValid())
	{
		if (PlaybackEventHandle.IsValid())
		{
			Subsystem->OnPlaybackEventNative().Remove(PlaybackEventHandle);
			PlaybackEventHandle.Reset();
		}
		Subsystem->UnregisterPlaybackObject(this);
	}
	Subsystem.Reset();
	if (PreparationLease)
	{
		PreparationLease->Release();
		PreparationLease = nullptr;
	}
}

FOpenMobileHapticControlResult
UOpenMobileHapticPlayback::MakeStaleControlResult() const
{
	FOpenMobileHapticControlResult Result;
	Result.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
	Result.State = State;
	Result.Error = FOpenMobileHapticError::FromCommon(
		EOpenMobileErrorCode::InvalidArgument,
		TEXT("This Haptic Playback no longer owns an active handle."),
		EOpenMobileHapticFailureStage::Playback
	);
	return Result;
}

void UOpenMobileHapticPlayback::ResolveControlResult(
	const FOpenMobileHapticControlResult& Result,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	Outcome = OpenMobileHapticPlaybackPrivate::ToBranch(Result.Outcome);
	Error = Result.Error;
	if (Result.State != EOpenMobileHapticPlaybackState::Invalid)
	{
		State = Result.State;
	}
}

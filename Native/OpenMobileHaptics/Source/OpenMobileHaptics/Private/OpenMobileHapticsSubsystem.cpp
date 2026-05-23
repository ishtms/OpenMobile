#include "OpenMobileHapticsSubsystem.h"

#include "Async/Async.h"
#include "IOpenMobileHapticsBackend.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsBackendRegistry.h"

struct FOpenMobileHapticsSubsystemRequestState
{
	FOpenMobileHapticsBackendRequestToken Token;
	uint64 LastCallbackSequence = 0;
};

struct FOpenMobileHapticsSubsystemState
{
	TMap<uint64, FOpenMobileHapticsSubsystemRequestState> Requests;
	TMap<FOpenMobileHapticPlaybackHandle, uint64> RequestByHandle;
	TMap<FOpenMobileHapticPlaybackHandle, EOpenMobileHapticPlaybackState>
		PlaybackStates;
	FOpenMobileError LastError;
};

void FOpenMobileHapticsSubsystemStateDeleter::operator()(
	FOpenMobileHapticsSubsystemState* State
) const
{
	delete State;
}

namespace OpenMobileHapticsSubsystemPrivate
{
	FOpenMobileHapticPlaybackResult MakeUnsupportedPlaybackResult()
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("Phone haptics are not supported on this platform.")
		);
	}

	FOpenMobileHapticControlResult MakeUnsupportedControlResult()
	{
		FOpenMobileHapticControlResult Result =
			FOpenMobileHapticControlResult::MakeRejected(
				EOpenMobileErrorCode::NotSupported,
				TEXT("Phone haptic controls are not supported on this platform.")
			);
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		return Result;
	}

	bool IsValidScaleMap(const TMap<FName, float>& Scales)
	{
		for (const TPair<FName, float>& Scale : Scales)
		{
			if (Scale.Key.IsNone()
				|| !FMath::IsFinite(Scale.Value)
				|| Scale.Value < 0.0f
				|| Scale.Value > 1.0f)
			{
				return false;
			}
		}
		return true;
	}

	bool IsTerminalState(EOpenMobileHapticPlaybackState State)
	{
		switch (State)
		{
		case EOpenMobileHapticPlaybackState::Stopped:
		case EOpenMobileHapticPlaybackState::Cancelled:
		case EOpenMobileHapticPlaybackState::Completed:
		case EOpenMobileHapticPlaybackState::Interrupted:
		case EOpenMobileHapticPlaybackState::Failed:
			return true;
		default:
			return false;
		}
	}

	void RemoveRequest(
		FOpenMobileHapticsSubsystemState& State,
		uint64 RequestId
	)
	{
		if (const FOpenMobileHapticsSubsystemRequestState* Request =
			State.Requests.Find(RequestId))
		{
			if (Request->Token.PlaybackHandle.IsValid())
			{
				State.RequestByHandle.Remove(Request->Token.PlaybackHandle);
			}
		}
		State.Requests.Remove(RequestId);
	}

	FOpenMobileHapticPlaybackResult FinalizeSubmission(
		FOpenMobileHapticsSubsystemState& State,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FName Channel,
		FOpenMobileHapticsBackendSubmission Submission
	)
	{
		FOpenMobileHapticPlaybackResult Result = MoveTemp(Submission.Result);
		Result.Channel = Channel;
		if (!Result.IsAccepted())
		{
			Result.Handle = {};
			RemoveRequest(State, Token.RequestId);
			if (Result.Error.IsSet())
			{
				State.LastError = Result.Error;
			}
			return Result;
		}

		if (!Token.IsValid()
			|| (Submission.bCreatesControllablePlayback
				&& (!Token.PlaybackHandle.IsValid()
					|| !Submission.bExpectsCallbacks)))
		{
			RemoveRequest(State, Token.RequestId);
			Result = FOpenMobileHapticPlaybackResult::MakeRejected(
				EOpenMobileErrorCode::Internal,
				TEXT("The Haptics backend returned an inconsistent acceptance contract.")
			);
			State.LastError = Result.Error;
			return Result;
		}

		if (Result.State == EOpenMobileHapticPlaybackState::Invalid)
		{
			Result.State = EOpenMobileHapticPlaybackState::Accepted;
		}
		if (Submission.bCreatesControllablePlayback)
		{
			Result.Handle = Token.PlaybackHandle;
			State.RequestByHandle.Add(Token.PlaybackHandle, Token.RequestId);
			State.PlaybackStates.Add(Token.PlaybackHandle, Result.State);
		}
		else
		{
			Result.Handle = {};
		}
		if (!Submission.bExpectsCallbacks)
		{
			RemoveRequest(State, Token.RequestId);
		}
		return Result;
	}
}

UOpenMobileHapticsSubsystem::~UOpenMobileHapticsSubsystem() = default;

void UOpenMobileHapticsSubsystem::Initialize(
	FSubsystemCollectionBase& Collection
)
{
	Super::Initialize(Collection);
	bDeinitialized = false;
	UserPolicy = {};
	State.Reset(new FOpenMobileHapticsSubsystemState());
}

void UOpenMobileHapticsSubsystem::Deinitialize()
{
	if (bDeinitialized)
	{
		return;
	}
	bDeinitialized = true;

	TArray<TWeakObjectPtr<UOpenMobileHapticPlaybackAsyncAction>> Actions;
	Actions.Reserve(ActiveAsyncActions.Num());
	for (const TWeakObjectPtr<UOpenMobileHapticPlaybackAsyncAction>& Action :
		ActiveAsyncActions)
	{
		Actions.Add(Action);
	}
	ActiveAsyncActions.Reset();
	for (const TWeakObjectPtr<UOpenMobileHapticPlaybackAsyncAction>& Action : Actions)
	{
		if (Action.IsValid())
		{
			Action->HandleGameInstanceTeardown();
		}
	}
	NativePlaybackEvent.Clear();
	State.Reset();

	Super::Deinitialize();
}

FOpenMobileHapticCapabilities
UOpenMobileHapticsSubsystem::GetHapticCapabilities() const
{
	return GetCapabilitiesNative();
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::PlaySemanticFeedback(
	EOpenMobileHapticSemanticEffect Effect,
	float Intensity,
	FName Channel
)
{
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = Channel.IsNone() ? FName(TEXT("UI")) : Channel;
	Options.Category = TEXT("UI");
	return PlaySemanticFeedbackAdvanced(Effect, Intensity, Options);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::PlaySemanticFeedbackAdvanced(
	EOpenMobileHapticSemanticEffect Effect,
	float Intensity,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	FOpenMobileHapticSemanticRequest Request;
	Request.Effect = Effect;
	Request.Intensity = Intensity;
	Request.Options = Options;
	return SubmitSemantic(Request);
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::Vibrate(
	float DurationSeconds,
	float Intensity,
	FName Channel
)
{
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = Channel.IsNone() ? FName(TEXT("Gameplay")) : Channel;
	return VibrateAdvanced(DurationSeconds, Intensity, Options);
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::VibrateAdvanced(
	float DurationSeconds,
	float Intensity,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	FOpenMobileHapticOneShotRequest Request;
	Request.DurationSeconds = DurationSeconds;
	Request.Intensity = Intensity;
	Request.Options = Options;
	return SubmitOneShot(Request);
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::PlayNamedPattern(
	FName PatternName,
	float Intensity,
	FName Channel
)
{
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = Channel.IsNone() ? FName(TEXT("Gameplay")) : Channel;
	return PlayNamedPatternAdvanced(PatternName, Intensity, Options);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::PlayNamedPatternAdvanced(
	FName PatternName,
	float Intensity,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	FOpenMobileHapticNamedPatternRequest Request;
	Request.PatternName = PatternName;
	Request.Intensity = Intensity;
	Request.Options = Options;
	return SubmitNamedPattern(Request);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::StopPlayback(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return StopPlaybackNative(Handle);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::StopChannel(
	FName Channel
)
{
	return StopChannelNative(Channel);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::StopAll()
{
	return StopAllNative();
}

EOpenMobileHapticPlaybackState UOpenMobileHapticsSubsystem::GetPlaybackState(
	FOpenMobileHapticPlaybackHandle Handle
) const
{
	return GetPlaybackStateNative(Handle);
}

FOpenMobileHapticUserPolicy UOpenMobileHapticsSubsystem::GetUserPolicy() const
{
	return GetUserPolicyNative();
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::SetUserPolicy(
	const FOpenMobileHapticUserPolicy& Policy
)
{
	return UpdateUserPolicy(Policy);
}

FOpenMobileHapticsDiagnostics UOpenMobileHapticsSubsystem::GetDiagnostics() const
{
	return GetDiagnosticsNative();
}

FOpenMobileHapticCapabilities
UOpenMobileHapticsSubsystem::GetCapabilitiesNative() const
{
	check(IsInGameThread());
	if (!bDeinitialized)
	{
		if (IOpenMobileHapticsBackend* Backend =
			FOpenMobileHapticsBackendRegistry::FindBackend())
		{
			FOpenMobileHapticCapabilities Capabilities =
				Backend->GetCapabilities();
			if (Capabilities.BackendName.IsNone())
			{
				Capabilities.BackendName = Backend->GetBackendName();
			}
			return Capabilities;
		}
	}

	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.Detail = TEXT("No mobile Haptics backend is available.");
	return Capabilities;
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitSemantic(
	const FOpenMobileHapticSemanticRequest& Request
)
{
	check(IsInGameThread());
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
	}

	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const FOpenMobileHapticsBackendRequestToken Token =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, false);
	LocalState.Requests.Add(Token.RequestId, {Token, 0});
	return OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
		LocalState,
		Token,
		Request.Options.Channel,
		Backend->SubmitSemantic(Request, Token, MakeBackendCallback())
	);
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitOneShot(
	const FOpenMobileHapticOneShotRequest& Request
)
{
	check(IsInGameThread());
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
	}

	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const FOpenMobileHapticsBackendRequestToken Token =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, true);
	LocalState.Requests.Add(Token.RequestId, {Token, 0});
	return OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
		LocalState,
		Token,
		Request.Options.Channel,
		Backend->SubmitOneShot(Request, Token, MakeBackendCallback())
	);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitNamedPattern(
	const FOpenMobileHapticNamedPatternRequest& Request
)
{
	check(IsInGameThread());
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
	}

	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const FOpenMobileHapticsBackendRequestToken Token =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, true);
	LocalState.Requests.Add(Token.RequestId, {Token, 0});
	return OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
		LocalState,
		Token,
		Request.Options.Channel,
		Backend->SubmitNamedPattern(Request, Token, MakeBackendCallback())
	);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::StopPlaybackNative(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	check(IsInGameThread());
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
	}

	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const uint64* RequestId = LocalState.RequestByHandle.Find(Handle);
	FOpenMobileHapticsSubsystemRequestState* Request = RequestId
		? LocalState.Requests.Find(*RequestId)
		: nullptr;
	if (!Request)
	{
		FOpenMobileHapticControlResult Result =
			FOpenMobileHapticControlResult::MakeRejected(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The Haptics playback handle is stale or unknown.")
			);
		Result.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
		return Result;
	}

	if (!FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(Request->Token)
		|| Backend->GetBackendName() != Request->Token.BackendName)
	{
		LocalState.PlaybackStates.Add(
			Handle,
			EOpenMobileHapticPlaybackState::Interrupted
		);
		OpenMobileHapticsSubsystemPrivate::RemoveRequest(
			LocalState,
			Request->Token.RequestId
		);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The backend that owns this Haptics handle is no longer active.")
		);
		return Result;
	}
	if (!Backend->GetControlSupport().bStop)
	{
		FOpenMobileHapticControlResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
		Result.Error.Message =
			TEXT("The active Haptics backend cannot stop individual playback.");
		return Result;
	}
	return Backend->StopPlayback(Request->Token);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::StopChannelNative(
	FName Channel
)
{
	check(IsInGameThread());
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend || !Backend->GetControlSupport().bStopChannel)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
	}
	return Backend->StopChannel(Channel);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::StopAllNative()
{
	check(IsInGameThread());
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend || !Backend->GetControlSupport().bStopAll)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
	}
	return Backend->StopAll();
}

EOpenMobileHapticPlaybackState
UOpenMobileHapticsSubsystem::GetPlaybackStateNative(
	FOpenMobileHapticPlaybackHandle Handle
) const
{
	check(IsInGameThread());
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	if (const EOpenMobileHapticPlaybackState* PlaybackState =
		LocalState.PlaybackStates.Find(Handle))
	{
		return *PlaybackState;
	}
	return EOpenMobileHapticPlaybackState::Invalid;
}

FOpenMobileHapticUserPolicy
UOpenMobileHapticsSubsystem::GetUserPolicyNative() const
{
	return UserPolicy;
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::UpdateUserPolicy(
	const FOpenMobileHapticUserPolicy& Policy
)
{
	if (!FMath::IsFinite(Policy.MasterIntensity)
		|| Policy.MasterIntensity < 0.0f
		|| Policy.MasterIntensity > 1.0f
		|| !OpenMobileHapticsSubsystemPrivate::IsValidScaleMap(
			Policy.CategoryScales
		)
		|| !OpenMobileHapticsSubsystemPrivate::IsValidScaleMap(
			Policy.EffectScales
		))
	{
		return FOpenMobileHapticControlResult::MakeRejected(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Haptics policy scales must be finite values from 0 to 1 with nonempty names.")
		);
	}

	UserPolicy = Policy;
	FOpenMobileHapticControlResult Result;
	Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
	return Result;
}

FOpenMobileHapticsDiagnostics
UOpenMobileHapticsSubsystem::GetDiagnosticsNative() const
{
	FOpenMobileHapticsDiagnostics Diagnostics;
	Diagnostics.Capabilities = GetCapabilitiesNative();
	if (State)
	{
		Diagnostics.ActivePlaybackCount = State->Requests.Num();
		Diagnostics.LastError = State->LastError;
	}
	return Diagnostics;
}

FOpenMobileHapticNativePlaybackEvent&
UOpenMobileHapticsSubsystem::OnPlaybackEventNative()
{
	return NativePlaybackEvent;
}

void UOpenMobileHapticsSubsystem::RegisterAsyncAction(
	UOpenMobileHapticPlaybackAsyncAction* Action
)
{
	if (!bDeinitialized && IsValid(Action))
	{
		ActiveAsyncActions.Add(Action);
	}
}

void UOpenMobileHapticsSubsystem::UnregisterAsyncAction(
	UOpenMobileHapticPlaybackAsyncAction* Action
)
{
	ActiveAsyncActions.Remove(Action);
}

FOpenMobileHapticsSubsystemState&
UOpenMobileHapticsSubsystem::GetOrCreateState() const
{
	if (!State)
	{
		State.Reset(new FOpenMobileHapticsSubsystemState());
	}
	return *State;
}

TFunction<void(const FOpenMobileHapticsBackendCallback&)>
UOpenMobileHapticsSubsystem::MakeBackendCallback()
{
	const TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakSubsystem(this);
	return [WeakSubsystem](const FOpenMobileHapticsBackendCallback& Callback)
	{
		FOpenMobileHapticsBackendCallback CallbackCopy = Callback;
		AsyncTask(
			ENamedThreads::GameThread,
			[WeakSubsystem, CallbackCopy = MoveTemp(CallbackCopy)]()
			{
				if (UOpenMobileHapticsSubsystem* Subsystem = WeakSubsystem.Get())
				{
					Subsystem->HandleBackendCallback(CallbackCopy);
				}
			}
		);
	};
}

void UOpenMobileHapticsSubsystem::HandleBackendCallback(
	const FOpenMobileHapticsBackendCallback& Callback
)
{
	check(IsInGameThread());
	if (bDeinitialized
		|| Callback.Sequence == 0
		|| Callback.Event.State == EOpenMobileHapticPlaybackState::Invalid
		|| !FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(Callback.Token))
	{
		return;
	}

	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	FOpenMobileHapticsSubsystemRequestState* Request =
		LocalState.Requests.Find(Callback.Token.RequestId);
	if (!Request
		|| !(Request->Token == Callback.Token)
		|| Callback.Sequence <= Request->LastCallbackSequence
		|| (Callback.Event.Handle.IsValid()
			&& Callback.Event.Handle != Callback.Token.PlaybackHandle))
	{
		return;
	}
	Request->LastCallbackSequence = Callback.Sequence;

	FOpenMobileHapticPlaybackEvent Event = Callback.Event;
	Event.Handle = Callback.Token.PlaybackHandle;
	if (Event.Handle.IsValid())
	{
		LocalState.PlaybackStates.Add(Event.Handle, Event.State);
	}
	if (Event.Error.IsSet())
	{
		LocalState.LastError = Event.Error;
	}
	if (OpenMobileHapticsSubsystemPrivate::IsTerminalState(Event.State))
	{
		OpenMobileHapticsSubsystemPrivate::RemoveRequest(
			LocalState,
			Callback.Token.RequestId
		);
	}

	OnPlaybackEvent.Broadcast(Event);
	NativePlaybackEvent.Broadcast(Event);
}

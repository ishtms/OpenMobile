#include "OpenMobileHapticsSubsystem.h"

#include "Async/Async.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobileHapticsBackend.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsErrorMapper.h"
#include "OpenMobileHapticsRateLimiter.h"
#include "OpenMobileHapticsSemanticPolicy.h"
#include "OpenMobileHapticsSettings.h"

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
	FOpenMobileHapticError LastError;
	FOpenMobileHapticsRateLimiter RateLimiter;
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
		FOpenMobileHapticsErrorContext Context;
		Context.Reason =
			EOpenMobileHapticsFailureReason::UnsupportedFeature;
		Context.Stage = EOpenMobileHapticFailureStage::Capability;
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			FOpenMobileHapticsErrorMapper::Map(Context)
		);
	}

	FOpenMobileHapticPlaybackResult MakeRejectedPlaybackResult(
		EOpenMobileHapticsFailureReason Reason,
		EOpenMobileHapticFailureStage Stage,
		FName Effect,
		FName Channel
	)
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = Reason;
		Context.Stage = Stage;
		Context.FailedItem = Effect;
		Context.Channel = Channel;
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			FOpenMobileHapticsErrorMapper::Map(Context)
		);
	}

	FOpenMobileHapticPlaybackResult MakeSuppressedPlaybackResult(
		FName Channel,
		FName Reason
	)
	{
		FOpenMobileHapticPlaybackResult Result;
		Result.Outcome = EOpenMobileHapticPlaybackOutcome::Suppressed;
		Result.State = EOpenMobileHapticPlaybackState::Completed;
		Result.Channel = Channel;
		Result.ResolvedPath = Reason;
		return Result;
	}

	float FindScale(const TMap<FName, float>& Scales, FName Name)
	{
		const float* Scale = Scales.Find(Name);
		return Scale ? *Scale : 1.0f;
	}

	FOpenMobileHapticControlResult MakeUnsupportedControlResult()
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason =
			EOpenMobileHapticsFailureReason::UnsupportedFeature;
		Context.Stage = EOpenMobileHapticFailureStage::Capability;
		FOpenMobileHapticControlResult Result =
			FOpenMobileHapticControlResult::MakeRejected(
				FOpenMobileHapticsErrorMapper::Map(Context)
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
		if (Result.Outcome == EOpenMobileHapticPlaybackOutcome::Suppressed)
		{
			Result.Handle = {};
			if (Result.State == EOpenMobileHapticPlaybackState::Invalid)
			{
				Result.State = EOpenMobileHapticPlaybackState::Completed;
			}
			RemoveRequest(State, Token.RequestId);
			return Result;
		}
		if (!Result.IsAccepted())
		{
			Result.Handle = {};
			RemoveRequest(State, Token.RequestId);
			FOpenMobileHapticsErrorContext Context;
			Context.Reason =
				EOpenMobileHapticsFailureReason::NativeEngineFailure;
			Context.Stage = EOpenMobileHapticFailureStage::NativeSubmission;
			Context.Channel = Channel;
			Result.Error = FOpenMobileHapticsErrorMapper::Complete(
				MoveTemp(Result.Error),
				Context
			);
			State.LastError = Result.Error;
			return Result;
		}

		if (!Token.IsValid()
			|| (Submission.bCreatesControllablePlayback
				&& (!Token.PlaybackHandle.IsValid()
					|| !Submission.bExpectsCallbacks)))
		{
			RemoveRequest(State, Token.RequestId);
			FOpenMobileHapticsErrorContext Context;
			Context.Reason = EOpenMobileHapticsFailureReason::Internal;
			Context.Stage = EOpenMobileHapticFailureStage::NativeSubmission;
			Context.Channel = Channel;
			Context.Handle = Token.PlaybackHandle;
			Result = FOpenMobileHapticPlaybackResult::MakeRejected(
				FOpenMobileHapticsErrorMapper::Map(Context)
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
	bUserPolicyEnabled.Store(UserPolicy.bEnabled);
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
UOpenMobileHapticsSubsystem::PlaySelectionFeedback(
	float Intensity,
	FName Channel
)
{
	return PlaySemanticFeedback(
		EOpenMobileHapticSemanticEffect::Selection,
		Intensity,
		Channel
	);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::PlayImpactFeedback(
	EOpenMobileHapticImpactStyle Style,
	float Intensity,
	FName Channel
)
{
	EOpenMobileHapticSemanticEffect Effect =
		static_cast<EOpenMobileHapticSemanticEffect>(MAX_uint8);
	switch (Style)
	{
	case EOpenMobileHapticImpactStyle::Light:
		Effect = EOpenMobileHapticSemanticEffect::ImpactLight;
		break;
	case EOpenMobileHapticImpactStyle::Medium:
		Effect = EOpenMobileHapticSemanticEffect::ImpactMedium;
		break;
	case EOpenMobileHapticImpactStyle::Heavy:
		Effect = EOpenMobileHapticSemanticEffect::ImpactHeavy;
		break;
	case EOpenMobileHapticImpactStyle::Soft:
		Effect = EOpenMobileHapticSemanticEffect::ImpactSoft;
		break;
	case EOpenMobileHapticImpactStyle::Rigid:
		Effect = EOpenMobileHapticSemanticEffect::ImpactRigid;
		break;
	default:
		break;
	}
	return PlaySemanticFeedback(Effect, Intensity, Channel);
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
	Options.Category =
		FOpenMobileHapticsSemanticPolicy::Describe(Effect).Category;
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
	FOpenMobileHapticCapabilities Capabilities =
		FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot();
	if (!bUserPolicyEnabled.Load()
		&& Capabilities.Availability
			!= EOpenMobileHapticAvailability::UnsupportedPlatform)
	{
		Capabilities.Availability =
			EOpenMobileHapticAvailability::DisabledByPolicy;
		Capabilities.Detail =
			TEXT("Haptics are disabled by the current player policy.");
	}
	return Capabilities;
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitSemantic(
	const FOpenMobileHapticSemanticRequest& Request
)
{
	check(IsInGameThread());
	const FOpenMobileHapticsSemanticDescriptor Descriptor =
		FOpenMobileHapticsSemanticPolicy::Describe(Request.Effect);
	if (static_cast<uint8>(Request.Effect)
			> static_cast<uint8>(EOpenMobileHapticSemanticEffect::Achievement)
		|| !FMath::IsFinite(Request.Intensity)
		|| Request.Intensity < 0.0f
		|| Request.Intensity > 1.0f
		|| !FMath::IsFinite(Request.Options.IntensityScale)
		|| Request.Options.IntensityScale < 0.0f
		|| Request.Options.IntensityScale > 1.0f
		|| Request.Options.Channel.IsNone()
		|| Request.Options.Category.IsNone())
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::InvalidRequest,
			EOpenMobileHapticFailureStage::Validation,
			Descriptor.Name,
			Request.Options.Channel
		);
	}
	if (!UserPolicy.bEnabled)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("PlayerPolicy")
		);
	}

	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	if (!FOpenMobileHapticsBackendRegistry::IsApplicationActive()
		&& Settings->BackgroundPolicy
			!= EOpenMobileHapticBackgroundPolicy::AllowAll
		&& (Settings->BackgroundPolicy
				!= EOpenMobileHapticBackgroundPolicy::CriticalOnly
			|| Request.Options.Priority
				!= EOpenMobileHapticChannelPriority::Critical))
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("BackgroundPolicy")
		);
	}

	FOpenMobileHapticSemanticRequest AdjustedRequest = Request;
	float ProjectScale = 1.0f;
	double MinimumIntervalSeconds = Settings->DefaultMinimumIntervalSeconds;
	for (const FOpenMobileHapticChannelSettings& Channel : Settings->Channels)
	{
		if (Channel.Name == Request.Options.Channel)
		{
			ProjectScale *= Channel.IntensityScale;
			MinimumIntervalSeconds = Channel.MinimumIntervalSeconds;
			break;
		}
	}
	for (const FOpenMobileHapticEffectSettings& Effect :
		Settings->EffectOverrides)
	{
		if (Effect.Name == Descriptor.Name)
		{
			ProjectScale *= Effect.IntensityScale;
			MinimumIntervalSeconds = FMath::Max<double>(
				MinimumIntervalSeconds,
				Effect.MinimumIntervalSeconds
			);
			break;
		}
	}
	AdjustedRequest.Intensity = Request.Intensity
		* Request.Options.IntensityScale
		* UserPolicy.MasterIntensity
		* OpenMobileHapticsSubsystemPrivate::FindScale(
			UserPolicy.CategoryScales,
			Request.Options.Category
		)
		* OpenMobileHapticsSubsystemPrivate::FindScale(
			UserPolicy.EffectScales,
			Descriptor.Name
		)
		* ProjectScale;
	if (AdjustedRequest.Intensity <= 0.0f)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("ZeroIntensity")
		);
	}

	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
	}

	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const FOpenMobileHapticsSemanticResolution Resolution =
		FOpenMobileHapticsSemanticPolicy::Resolve(
			FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot(),
			Request.Options.FallbackPolicy
		);
	if (Resolution.Path == EOpenMobileHapticsSemanticPath::Unsupported)
	{
		if (Resolution.bSuppressWhenUnavailable)
		{
			return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
				Request.Options.Channel,
				TEXT("Unavailable")
			);
		}
		return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::UnsupportedFeature,
			EOpenMobileHapticFailureStage::Capability,
			Descriptor.Name,
			Request.Options.Channel
		);
	}
	const bool bSelection = Descriptor.Behavior
		== EOpenMobileHapticsSemanticBehavior::Selection;
	if (LocalState.RateLimiter.ShouldSuppress(
		Request.Options.Channel,
		bSelection,
		FPlatformTime::Seconds(),
		MinimumIntervalSeconds,
		Settings->SelectionDebounceSeconds,
		Settings->MaximumSubmissionsPerSecond
	))
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("RateLimited")
		);
	}
	const FOpenMobileHapticsBackendRequestToken Token =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, false);
	LocalState.Requests.Add(Token.RequestId, {Token, 0});
	FOpenMobileHapticPlaybackResult Result =
		OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
		LocalState,
		Token,
		Request.Options.Channel,
		Backend->SubmitSemantic(
			AdjustedRequest,
			Resolution,
			Token,
			MakeBackendCallback()
		)
	);
	if (Result.IsAccepted())
	{
		if (Result.ResolvedPath.IsNone())
		{
			Result.ResolvedPath =
				FOpenMobileHapticsSemanticPolicy::PathName(Resolution.Path);
		}
		if (Resolution.bFallback)
		{
			Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
		}
	}
	return Result;
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
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = EOpenMobileHapticsFailureReason::BackendUnavailable;
		Context.Stage = EOpenMobileHapticFailureStage::Playback;
		Context.Handle = Handle;
		Context.bAfterAcceptance = Handle.IsValid();
		FOpenMobileHapticControlResult Result =
			FOpenMobileHapticControlResult::MakeRejected(
				FOpenMobileHapticsErrorMapper::Map(Context)
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
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = EOpenMobileHapticsFailureReason::BackendUnavailable;
		Context.Stage = EOpenMobileHapticFailureStage::Playback;
		Context.Handle = Handle;
		Context.bAfterAcceptance = true;
		Result.Error = FOpenMobileHapticsErrorMapper::Map(Context);
		return Result;
	}
	if (!Backend->GetControlSupport().bStop)
	{
		FOpenMobileHapticControlResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
		Result.Error.Handle = Handle;
		Result.Error.bRejectedBeforeSubmission = false;
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
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = EOpenMobileHapticsFailureReason::InvalidRequest;
		Context.Stage = EOpenMobileHapticFailureStage::Policy;
		return FOpenMobileHapticControlResult::MakeRejected(
			FOpenMobileHapticsErrorMapper::Map(Context)
		);
	}

	UserPolicy = Policy;
	bUserPolicyEnabled.Store(Policy.bEnabled);
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
	if (Event.State == EOpenMobileHapticPlaybackState::Interrupted
		|| Event.State == EOpenMobileHapticPlaybackState::Failed)
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason =
			Event.State == EOpenMobileHapticPlaybackState::Interrupted
				? EOpenMobileHapticsFailureReason::Interrupted
				: EOpenMobileHapticsFailureReason::NativeEngineFailure;
		Context.Stage =
			Event.State == EOpenMobileHapticPlaybackState::Interrupted
				? EOpenMobileHapticFailureStage::Interruption
				: EOpenMobileHapticFailureStage::Playback;
		Context.FailedItem = Event.PatternOrEffect;
		Context.Channel = Event.Channel;
		Context.Handle = Event.Handle;
		Context.bAfterAcceptance = true;
		Event.Error = FOpenMobileHapticsErrorMapper::Complete(
			MoveTemp(Event.Error),
			Context
		);
	}
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

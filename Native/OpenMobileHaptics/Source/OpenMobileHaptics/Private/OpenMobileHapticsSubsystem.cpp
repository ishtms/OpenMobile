#include "OpenMobileHapticsSubsystem.h"

#include "OpenMobileHapticsAsyncAction.h"

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
}

void UOpenMobileHapticsSubsystem::Initialize(
	FSubsystemCollectionBase& Collection
)
{
	Super::Initialize(Collection);
	bDeinitialized = false;
	UserPolicy = {};
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
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.Detail = TEXT("No mobile Haptics backend is available.");
	return Capabilities;
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitSemantic(
	const FOpenMobileHapticSemanticRequest& Request
)
{
	static_cast<void>(Request);
	return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitOneShot(
	const FOpenMobileHapticOneShotRequest& Request
)
{
	static_cast<void>(Request);
	return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitNamedPattern(
	const FOpenMobileHapticNamedPatternRequest& Request
)
{
	static_cast<void>(Request);
	return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::StopPlaybackNative(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	static_cast<void>(Handle);
	return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::StopChannelNative(
	FName Channel
)
{
	static_cast<void>(Channel);
	return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::StopAllNative()
{
	return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
}

EOpenMobileHapticPlaybackState
UOpenMobileHapticsSubsystem::GetPlaybackStateNative(
	FOpenMobileHapticPlaybackHandle Handle
) const
{
	static_cast<void>(Handle);
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

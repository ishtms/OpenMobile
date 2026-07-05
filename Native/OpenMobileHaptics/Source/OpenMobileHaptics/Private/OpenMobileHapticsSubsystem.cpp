#include "OpenMobileHapticsSubsystem.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobileHapticsBackend.h"
#include "OpenMobileHapticLibrary.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsDurationPolicy.h"
#include "OpenMobileHapticsDynamicParameterPolicy.h"
#include "OpenMobileHapticsErrorMapper.h"
#include "OpenMobileHapticsIntensityPolicy.h"
#include "OpenMobileHapticsLibraryResolver.h"
#include "OpenMobileHapticsOneShotPolicy.h"
#include "OpenMobileHapticsRateLimiter.h"
#include "OpenMobileHapticsSemanticPolicy.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsTimingPolicy.h"
#include "OpenMobileHapticsTimelineManager.h"

struct FOpenMobileHapticsSubsystemRequestState
{
	FOpenMobileHapticsBackendRequestToken Token;
	uint64 LastCallbackSequence = 0;
	FName Channel;
	FName Category;
	FName Effect;
	float RuntimeIntensity = 1.0f;
	float RuntimeSharpness = 0.5f;
	bool bSupportsDynamicParameters = false;
};

struct FOpenMobileHapticsSubsystemState
{
	TMap<uint64, FOpenMobileHapticsSubsystemRequestState> Requests;
	TMap<FOpenMobileHapticPlaybackHandle, uint64> RequestByHandle;
	TMap<FOpenMobileHapticPlaybackHandle, EOpenMobileHapticPlaybackState>
		PlaybackStates;
	FOpenMobileHapticError LastError;
	FOpenMobileHapticDurationDiagnostics LastDuration;
	FOpenMobileHapticIntensityDiagnostics LastIntensity;
	FName LastResolvedPath;
	TArray<FName> LastFallbackAttempts;
	FOpenMobileHapticsLibraryResolver LibraryResolver;
	TSharedPtr<FStreamableHandle> LibraryLoadHandle;
	TSharedPtr<FStreamableHandle> PatternLoadHandle;
	TSharedPtr<FStreamableHandle> OverrideLoadHandle;
	TArray<FSoftObjectPath> LoadingLibraryPaths;
	TArray<TWeakObjectPtr<UOpenMobileHapticLibrary>> LoadingLibraries;
	FOpenMobileHapticLibraryPreloadHandle ActiveLibraryPreload;
	FName LastNamedPattern;
	EOpenMobileHapticNamedPatternStatus LastNamedPatternStatus =
		EOpenMobileHapticNamedPatternStatus::Unprepared;
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;
	FOpenMobileHapticsRateLimiter RateLimiter;
	FOpenMobileHapticsDynamicParameterPolicy DynamicParameterPolicy;
	FOpenMobileHapticsTimingPolicy TimingPolicy;
	FTSTicker::FDelegateHandle DynamicParameterTickerHandle;
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

	FOpenMobileHapticPlaybackResult MakeTimingRejectedPlaybackResult(
		EOpenMobileHapticsTimingOutcome Outcome,
		FName Effect,
		FName Channel
	)
	{
		EOpenMobileHapticsFailureReason Reason =
			EOpenMobileHapticsFailureReason::InvalidRequest;
		EOpenMobileHapticFailureStage Stage =
			EOpenMobileHapticFailureStage::Validation;
		if (Outcome == EOpenMobileHapticsTimingOutcome::MissingCalibration
			|| Outcome
				== EOpenMobileHapticsTimingOutcome::StaleCalibration
			|| Outcome
				== EOpenMobileHapticsTimingOutcome::ClockDiscontinuity)
		{
			Reason = EOpenMobileHapticsFailureReason::NotConfigured;
			Stage = EOpenMobileHapticFailureStage::Preparation;
		}
		FOpenMobileHapticPlaybackResult Result = MakeRejectedPlaybackResult(
			Reason,
			Stage,
			Effect,
			Channel
		);
		if (Outcome == EOpenMobileHapticsTimingOutcome::MissingCalibration
			|| Outcome
				== EOpenMobileHapticsTimingOutcome::StaleCalibration)
		{
			Result.Error.Message = TEXT(
				"Calibrate the selected timing clock before absolute scheduling."
			);
		}
		else if (Outcome == EOpenMobileHapticsTimingOutcome::TooLate)
		{
			Result.Error.Message = TEXT(
				"The requested Haptics start time is too far in the past."
			);
		}
		else if (Outcome == EOpenMobileHapticsTimingOutcome::TooFar)
		{
			Result.Error.Message = TEXT(
				"The requested Haptics start time exceeds the scheduling horizon."
			);
		}
		return Result;
	}

	float FindScale(const TMap<FName, float>& Scales, FName Name)
	{
		const float* Scale = Scales.Find(Name);
		return Scale ? *Scale : 1.0f;
	}

	float ActivePolicyScale(
		const FOpenMobileHapticUserPolicy& Policy,
		const FOpenMobileHapticsSubsystemRequestState& Request
	)
	{
		if (!Policy.bEnabled)
		{
			return 0.0f;
		}
		return FOpenMobileHapticsIntensityPolicy::Scale(
			1.0f,
			Policy.MasterIntensity,
			FindScale(Policy.CategoryScales, Request.Category),
			FindScale(Policy.EffectScales, Request.Effect),
			1.0f,
			1.0f
		);
	}

	double DynamicParameterInterval(
		const UOpenMobileHapticsSettings& Settings
	)
	{
		return 1.0 / static_cast<double>(FMath::Clamp(
			Settings.MaximumDynamicParameterUpdatesPerSecond,
			1,
			240
		));
	}

	FOpenMobileHapticsPreparedResourceLimits PreparedResourceLimits(
		const UOpenMobileHapticsSettings& Settings
	)
	{
		FOpenMobileHapticsPreparedResourceLimits Limits;
		Limits.MaximumCount = Settings.MaximumPreparedPatterns;
		Limits.MaximumBytes = static_cast<int64>(
			Settings.MaximumPreparedPatternMemoryKilobytes
		) * 1024;
		Limits.IdleLifetimeSeconds =
			Settings.PreparedPatternIdleLifetimeSeconds;
		return Limits;
	}

	EOpenMobileHapticSemanticEffect GamePresetEffect(
		EOpenMobileHapticGamePreset Preset
	)
	{
		switch (Preset)
		{
		case EOpenMobileHapticGamePreset::Confirm:
			return EOpenMobileHapticSemanticEffect::Confirm;
		case EOpenMobileHapticGamePreset::Reject:
			return EOpenMobileHapticSemanticEffect::Reject;
		case EOpenMobileHapticGamePreset::Tick:
			return EOpenMobileHapticSemanticEffect::Tick;
		case EOpenMobileHapticGamePreset::Click:
			return EOpenMobileHapticSemanticEffect::Click;
		case EOpenMobileHapticGamePreset::Bump:
			return EOpenMobileHapticSemanticEffect::Bump;
		case EOpenMobileHapticGamePreset::Damage:
			return EOpenMobileHapticSemanticEffect::Damage;
		case EOpenMobileHapticGamePreset::Pickup:
			return EOpenMobileHapticSemanticEffect::Pickup;
		case EOpenMobileHapticGamePreset::Achievement:
			return EOpenMobileHapticSemanticEffect::Achievement;
		default:
			return static_cast<EOpenMobileHapticSemanticEffect>(MAX_uint8);
		}
	}

	FName FindLoadedGamePresetOverride(
		const UOpenMobileHapticsSettings& Settings,
		EOpenMobileHapticGamePreset Preset
	)
	{
		for (const FOpenMobileHapticNamedLibrarySettings& LibrarySettings :
			Settings.NamedLibraries)
		{
			const UOpenMobileHapticLibrary* Library =
				Cast<UOpenMobileHapticLibrary>(
					LibrarySettings.Asset.ResolveObject()
				);
			FName PatternName;
			if (Library
				&& Library->FindGamePresetOverride(Preset, PatternName))
			{
				return PatternName;
			}
		}
		return NAME_None;
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
		State.DynamicParameterPolicy.RemovePlayback(RequestId);
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
		State.LastResolvedPath = Result.ResolvedPath;
		State.LastFallbackAttempts = Result.FallbackAttempts;
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
			const FOpenMobileHapticsSubsystemRequestState* Request =
				State.Requests.Find(Token.RequestId);
			if (Request && Request->bSupportsDynamicParameters)
			{
				State.DynamicParameterPolicy.RegisterPlayback(
					Token.RequestId,
					FPlatformTime::Seconds()
				);
			}
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
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	UserPolicy = {};
	UserPolicy.bEnabled = Settings->bEnabledByDefault;
	UserPolicy.MasterIntensity = Settings->DefaultMasterIntensity;
	bUserPolicyEnabled.Store(UserPolicy.bEnabled);
	State.Reset(new FOpenMobileHapticsSubsystemState());
}

void UOpenMobileHapticsSubsystem::Deinitialize()
{
	if (bDeinitialized)
	{
		return;
	}
	if (State && State->DynamicParameterTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(
			State->DynamicParameterTickerHandle
		);
		State->DynamicParameterTickerHandle.Reset();
	}
	if (IOpenMobileHapticsBackend* Backend =
		FOpenMobileHapticsBackendRegistry::FindBackend())
	{
		if (State
			&& !State->Requests.IsEmpty()
			&& Backend->GetControlSupport().bStopAll)
		{
			Backend->StopAll();
		}
	}
	ReleaseNamedLibrariesInternal(false);
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
	OnNamedLibrariesPrepared.Clear();
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
UOpenMobileHapticsSubsystem::PlayNotificationFeedback(
	EOpenMobileHapticNotificationType Type,
	float Intensity,
	FName Channel
)
{
	EOpenMobileHapticSemanticEffect Effect =
		static_cast<EOpenMobileHapticSemanticEffect>(MAX_uint8);
	switch (Type)
	{
	case EOpenMobileHapticNotificationType::Success:
		Effect = EOpenMobileHapticSemanticEffect::NotificationSuccess;
		break;
	case EOpenMobileHapticNotificationType::Warning:
		Effect = EOpenMobileHapticSemanticEffect::NotificationWarning;
		break;
	case EOpenMobileHapticNotificationType::Error:
		Effect = EOpenMobileHapticSemanticEffect::NotificationError;
		break;
	default:
		break;
	}
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = Channel.IsNone() ? FName(TEXT("Alerts")) : Channel;
	Options.Category = TEXT("Alerts");
	return PlaySemanticFeedbackAdvanced(Effect, Intensity, Options);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::PlayGameFeedback(
	EOpenMobileHapticGamePreset Preset,
	float Intensity,
	FName Channel
)
{
	const EOpenMobileHapticSemanticEffect Effect =
		OpenMobileHapticsSubsystemPrivate::GamePresetEffect(Preset);
	const FOpenMobileHapticsSemanticDescriptor Descriptor =
		FOpenMobileHapticsSemanticPolicy::Describe(Effect);
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = Channel.IsNone() ? Descriptor.Category : Channel;
	Options.Category = Descriptor.Category;
	return PlayGameFeedbackAdvanced(Preset, Intensity, Options);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::PlayGameFeedbackAdvanced(
	EOpenMobileHapticGamePreset Preset,
	float Intensity,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	FOpenMobileHapticSemanticRequest Request;
	Request.Effect =
		OpenMobileHapticsSubsystemPrivate::GamePresetEffect(Preset);
	Request.Intensity = Intensity;
	Request.Options = Options;
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	return SubmitSemanticOrOverride(
		Request,
		OpenMobileHapticsSubsystemPrivate::FindLoadedGamePresetOverride(
			*Settings,
			Preset
		)
	);
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

FOpenMobileHapticTimingCalibrationResult
UOpenMobileHapticsSubsystem::CalibrateTimingClock(
	EOpenMobileHapticTimingClock Clock,
	double ClockTimeSeconds,
	double EstimatedPrecisionSeconds
)
{
	check(IsInGameThread());
	return GetOrCreateState().TimingPolicy.Calibrate(
		Clock,
		ClockTimeSeconds,
		FPlatformTime::Seconds(),
		EstimatedPrecisionSeconds,
		static_cast<int64>(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
		)
	);
}

FOpenMobileHapticLibraryPreloadHandle
UOpenMobileHapticsSubsystem::PreloadNamedLibraries()
{
	check(IsInGameThread());
	FOpenMobileHapticLibraryPreloadHandle Handle;
	if (bDeinitialized)
	{
		return Handle;
	}
	if (State && State->ActiveLibraryPreload.IsValid())
	{
		return State->ActiveLibraryPreload;
	}
	if (State
		&& State->PreparationState
			== EOpenMobileHapticPreparationState::Prepared)
	{
		Handle.Id = FGuid::NewGuid();
		State->ActiveLibraryPreload = Handle;
		TArray<FString> Errors;
		const bool bRequiresNativePreparation =
			FOpenMobileHapticsBackendRegistry::FindBackend()
			&& GetPreparationState()
				!= EOpenMobileHapticPreparationState::Prepared;
		if (bRequiresNativePreparation)
		{
			State->PreparationState =
				EOpenMobileHapticPreparationState::Preparing;
			const bool bPrepared = PrepareResolvedResources(Errors);
			TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakThis(this);
			AsyncTask(
				ENamedThreads::GameThread,
				[WeakThis, Handle, bPrepared, Errors = MoveTemp(Errors)]()
				mutable
				{
					if (WeakThis.IsValid())
					{
						WeakThis->FinishNamedLibraryPreload(
							Handle,
							bPrepared
								? EOpenMobileHapticLibraryPreloadOutcome::Prepared
								: EOpenMobileHapticLibraryPreloadOutcome::Failed,
							MoveTemp(Errors)
						);
					}
				}
			);
			return Handle;
		}
		TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Handle]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->FinishNamedLibraryPreload(
					Handle,
					EOpenMobileHapticLibraryPreloadOutcome::Prepared,
					{}
				);
			}
		});
		return Handle;
	}

	ReleaseNamedLibrariesInternal(true);
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	Handle.Id = FGuid::NewGuid();
	LocalState.ActiveLibraryPreload = Handle;
	const uint64 Generation = LocalState.LibraryResolver.BeginPreparation();
	LocalState.LastNamedPatternStatus =
		EOpenMobileHapticNamedPatternStatus::Loading;
	LocalState.PreparationState =
		EOpenMobileHapticPreparationState::Preparing;

	TArray<FSoftObjectPath> LibraryPaths;
	for (const FOpenMobileHapticNamedLibrarySettings& Library :
		GetDefault<UOpenMobileHapticsSettings>()->NamedLibraries)
	{
		LocalState.LoadingLibraryPaths.Add(Library.Asset);
		if (!Library.Asset.IsNull())
		{
			LibraryPaths.AddUnique(Library.Asset);
		}
	}
	if (LibraryPaths.IsEmpty())
	{
		TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakThis(this);
		AsyncTask(
			ENamedThreads::GameThread,
			[WeakThis, Generation, Handle]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleNamedLibrariesLoaded(
						Generation,
						Handle
					);
				}
			}
		);
		return Handle;
	}
	LocalState.LibraryLoadHandle =
		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			MoveTemp(LibraryPaths),
			FStreamableDelegate::CreateUObject(
				this,
				&UOpenMobileHapticsSubsystem::HandleNamedLibrariesLoaded,
				Generation,
				Handle
			),
			FStreamableManager::DefaultAsyncLoadPriority,
			false,
			false,
			TEXT("OpenMobile Haptics named libraries")
		);
	return Handle;
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::CancelNamedLibraryPreload(
	FOpenMobileHapticLibraryPreloadHandle Handle
)
{
	check(IsInGameThread());
	if (!State || !Handle.IsValid()
		|| State->ActiveLibraryPreload != Handle)
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
		return Result;
	}
	ReleaseNamedLibrariesInternal(true);
	FOpenMobileHapticControlResult Result;
	Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
	return Result;
}

void UOpenMobileHapticsSubsystem::ReleaseNamedLibraries()
{
	check(IsInGameThread());
	ReleaseNamedLibrariesInternal(true);
}

EOpenMobileHapticNamedPatternStatus
UOpenMobileHapticsSubsystem::GetNamedPatternStatus(FName PatternName) const
{
	check(IsInGameThread());
	return State
		? State->LibraryResolver.GetStatus(PatternName)
		: EOpenMobileHapticNamedPatternStatus::Unprepared;
}

EOpenMobileHapticPreparationState
UOpenMobileHapticsSubsystem::GetPreparationState() const
{
	check(IsInGameThread());
	if (!State)
	{
		return EOpenMobileHapticPreparationState::Unprepared;
	}
	if (State->PreparationState
		== EOpenMobileHapticPreparationState::Prepared)
	{
		if (IOpenMobileHapticsBackend* Backend =
			FOpenMobileHapticsBackendRegistry::FindBackend())
		{
			return Backend->GetPreparationState();
		}
	}
	return State->PreparationState;
}

bool UOpenMobileHapticsSubsystem::PrepareLoadedNamedLibraries(
	const TArray<UOpenMobileHapticLibrary*>& Libraries,
	TArray<FString>& Errors
)
{
	check(IsInGameThread());
	if (State
		&& GetPreparationState()
			== EOpenMobileHapticPreparationState::Prepared)
	{
		Errors.Reset();
		return true;
	}
	ReleaseNamedLibrariesInternal(false);
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	LocalState.PreparationState =
		EOpenMobileHapticPreparationState::Preparing;
	const uint64 Generation = LocalState.LibraryResolver.BeginPreparation();
	bool bPrepared = LocalState.LibraryResolver.CompletePreparation(
		Generation,
		Libraries,
		Errors
	);
	if (bPrepared)
	{
		bPrepared = PrepareResolvedResources(Errors);
	}
	else
	{
		LocalState.PreparationState =
			EOpenMobileHapticPreparationState::Failed;
	}
	LocalState.LastNamedPatternStatus = bPrepared
		? EOpenMobileHapticNamedPatternStatus::Loaded
		: EOpenMobileHapticNamedPatternStatus::Invalid;
	return bPrepared;
}

bool UOpenMobileHapticsSubsystem::PrepareResolvedResources(
	TArray<FString>& Errors
)
{
	check(IsInGameThread());
	if (!State)
	{
		Errors = {TEXT("Haptics preparation state is unavailable.")};
		return false;
	}
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const FOpenMobileHapticsPreparedResourceLimits Limits =
		OpenMobileHapticsSubsystemPrivate::PreparedResourceLimits(*Settings);
	FOpenMobileHapticsTimelineManager& TimelineManager =
		FOpenMobileHapticsBackendRegistry::GetTimelineManager();
	TimelineManager.SetLimits(Limits);
	TimelineManager.PruneIdle(FPlatformTime::Seconds());

	IOpenMobileHapticsBackend* Backend =
		FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		State->PreparationState =
			EOpenMobileHapticPreparationState::Prepared;
		return true;
	}

	FOpenMobileHapticsBackendPreparationRequest Request;
	Request.Limits = Limits;
	TArray<TPair<FName, FSoftObjectPath>> PreparedPatterns;
	State->LibraryResolver.GetPreparedPatterns(PreparedPatterns);
	const FOpenMobileHapticCapabilities Capabilities =
		FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot();
	TSet<uint64> PreparedResourceIds;
	for (const TPair<FName, FSoftObjectPath>& Prepared : PreparedPatterns)
	{
		const UOpenMobileHapticPatternAsset* Pattern =
			Cast<UOpenMobileHapticPatternAsset>(Prepared.Value.ResolveObject());
		if (!Pattern)
		{
			Errors.Add(FString::Printf(
				TEXT("Prepared pattern %s became unavailable."),
				*Prepared.Key.ToString()
			));
			continue;
		}
		const FOpenMobileHapticsTimelineLookup Lookup =
			TimelineManager.Resolve(
				Backend->GetBackendName(),
				*Pattern,
				Pattern->Loop,
				Capabilities,
				1.0f,
				EOpenMobileHapticFallbackPolicy::Automatic,
				FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
			);
		if (Lookup.Timeline
			&& Lookup.Timeline->ResourceId != 0
			&& !PreparedResourceIds.Contains(Lookup.Timeline->ResourceId))
		{
			PreparedResourceIds.Add(Lookup.Timeline->ResourceId);
			Request.Patterns.Add(Lookup.Timeline);
		}
	}
	if (!Errors.IsEmpty())
	{
		State->LibraryResolver.FailPreparation();
		State->PreparationState = EOpenMobileHapticPreparationState::Failed;
		TimelineManager.Clear();
		return false;
	}

	const FOpenMobileHapticsBackendPreparationResult Result =
		Backend->PrepareResources(Request);
	Errors = Result.Errors;
	State->PreparationState = Result.State;
	if (Result.State != EOpenMobileHapticPreparationState::Prepared)
	{
		if (Errors.IsEmpty())
		{
			Errors.Add(TEXT("The active Haptics backend could not prepare resources."));
		}
		State->LibraryResolver.FailPreparation();
		State->PreparationState = EOpenMobileHapticPreparationState::Failed;
		TimelineManager.Clear();
		return false;
	}
	return true;
}

void UOpenMobileHapticsSubsystem::HandleNamedLibrariesLoaded(
	uint64 Generation,
	FOpenMobileHapticLibraryPreloadHandle Handle
)
{
	check(IsInGameThread());
	if (bDeinitialized || !State
		|| State->ActiveLibraryPreload != Handle
		|| State->LibraryResolver.GetGeneration() != Generation)
	{
		return;
	}

	State->LoadingLibraries.Reset();
	TArray<FSoftObjectPath> PatternPaths;
	for (const FSoftObjectPath& LibraryPath : State->LoadingLibraryPaths)
	{
		UOpenMobileHapticLibrary* Library =
			Cast<UOpenMobileHapticLibrary>(LibraryPath.ResolveObject());
		State->LoadingLibraries.Add(Library);
		if (!Library)
		{
			continue;
		}
		for (const FOpenMobileHapticLibraryEntry& Entry : Library->Patterns)
		{
			if (!Entry.Pattern.IsNull())
			{
				PatternPaths.AddUnique(Entry.Pattern.ToSoftObjectPath());
			}
		}
	}

	if (PatternPaths.IsEmpty())
	{
		HandleNamedPatternsLoaded(Generation, Handle);
		return;
	}
	State->PatternLoadHandle =
		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			MoveTemp(PatternPaths),
			FStreamableDelegate::CreateUObject(
				this,
				&UOpenMobileHapticsSubsystem::HandleNamedPatternsLoaded,
				Generation,
				Handle
			),
			FStreamableManager::DefaultAsyncLoadPriority,
			false,
			false,
			TEXT("OpenMobile Haptics named patterns")
		);
}

void UOpenMobileHapticsSubsystem::HandleNamedPatternsLoaded(
	uint64 Generation,
	FOpenMobileHapticLibraryPreloadHandle Handle
)
{
	check(IsInGameThread());
	if (bDeinitialized || !State
		|| State->ActiveLibraryPreload != Handle
		|| State->LibraryResolver.GetGeneration() != Generation)
	{
		return;
	}
	TArray<FSoftObjectPath> OverridePaths;
	for (const TWeakObjectPtr<UOpenMobileHapticLibrary>& Library :
		State->LoadingLibraries)
	{
		if (!Library.IsValid())
		{
			continue;
		}
		for (const FOpenMobileHapticLibraryEntry& Entry : Library->Patterns)
		{
			const UOpenMobileHapticPatternAsset* Pattern = Entry.Pattern.Get();
			if (Pattern)
			{
				const FSoftObjectPath Override =
					Pattern->GetOverrideForCurrentPlatform();
				if (!Override.IsNull())
				{
					OverridePaths.AddUnique(Override);
				}
			}
		}
	}
	if (OverridePaths.IsEmpty())
	{
		HandleNamedOverridesLoaded(Generation, Handle);
		return;
	}
	State->OverrideLoadHandle =
		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			MoveTemp(OverridePaths),
			FStreamableDelegate::CreateUObject(
				this,
				&UOpenMobileHapticsSubsystem::HandleNamedOverridesLoaded,
				Generation,
				Handle
			),
			FStreamableManager::DefaultAsyncLoadPriority,
			false,
			false,
			TEXT("OpenMobile Haptics platform overrides")
		);
}

void UOpenMobileHapticsSubsystem::HandleNamedOverridesLoaded(
	uint64 Generation,
	FOpenMobileHapticLibraryPreloadHandle Handle
)
{
	check(IsInGameThread());
	if (bDeinitialized || !State
		|| State->ActiveLibraryPreload != Handle
		|| State->LibraryResolver.GetGeneration() != Generation)
	{
		return;
	}

	TArray<UOpenMobileHapticLibrary*> LoadedLibraries;
	LoadedLibraries.Reserve(State->LoadingLibraries.Num());
	for (const TWeakObjectPtr<UOpenMobileHapticLibrary>& Library :
		State->LoadingLibraries)
	{
		LoadedLibraries.Add(Library.Get());
	}
	TArray<FString> Errors;
	bool bPrepared = State->LibraryResolver.CompletePreparation(
		Generation,
		LoadedLibraries,
		Errors
	);
	if (bPrepared)
	{
		bPrepared = PrepareResolvedResources(Errors);
	}
	else
	{
		State->PreparationState =
			EOpenMobileHapticPreparationState::Failed;
	}
	State->LastNamedPatternStatus = bPrepared
		? EOpenMobileHapticNamedPatternStatus::Loaded
		: EOpenMobileHapticNamedPatternStatus::Invalid;
	FinishNamedLibraryPreload(
		Handle,
		bPrepared
			? EOpenMobileHapticLibraryPreloadOutcome::Prepared
			: EOpenMobileHapticLibraryPreloadOutcome::Failed,
		MoveTemp(Errors)
	);
}

void UOpenMobileHapticsSubsystem::FinishNamedLibraryPreload(
	FOpenMobileHapticLibraryPreloadHandle Handle,
	EOpenMobileHapticLibraryPreloadOutcome Outcome,
	TArray<FString> Errors
)
{
	if (!State || State->ActiveLibraryPreload != Handle)
	{
		return;
	}
	FOpenMobileHapticLibraryPreloadResult Result;
	Result.Handle = Handle;
	Result.Outcome = Outcome;
	Result.PreparedPatternCount =
		State->LibraryResolver.GetPreparedPatternCount();
	Result.Errors = MoveTemp(Errors);
	State->PreparationState = Outcome
		== EOpenMobileHapticLibraryPreloadOutcome::Prepared
			? EOpenMobileHapticPreparationState::Prepared
			: Outcome == EOpenMobileHapticLibraryPreloadOutcome::Cancelled
				? EOpenMobileHapticPreparationState::Unprepared
				: EOpenMobileHapticPreparationState::Failed;
	State->ActiveLibraryPreload = {};
	State->LoadingLibraryPaths.Reset();
	State->LoadingLibraries.Reset();
	if (Outcome != EOpenMobileHapticLibraryPreloadOutcome::Prepared)
	{
		if (State->LibraryLoadHandle)
		{
			State->LibraryLoadHandle->ReleaseHandle();
			State->LibraryLoadHandle.Reset();
		}
		if (State->PatternLoadHandle)
		{
			State->PatternLoadHandle->ReleaseHandle();
			State->PatternLoadHandle.Reset();
		}
		if (State->OverrideLoadHandle)
		{
			State->OverrideLoadHandle->ReleaseHandle();
			State->OverrideLoadHandle.Reset();
		}
	}
	OnNamedLibrariesPrepared.Broadcast(Result);
}

void UOpenMobileHapticsSubsystem::ReleaseNamedLibrariesInternal(
	bool bNotifyCancellation
)
{
	if (!State)
	{
		return;
	}
	const FOpenMobileHapticLibraryPreloadHandle ActiveHandle =
		State->ActiveLibraryPreload;
	const EOpenMobileHapticPreparationState PreviousPreparationState =
		State->PreparationState;
	if (State->LibraryLoadHandle)
	{
		State->LibraryLoadHandle->CancelHandle();
		State->LibraryLoadHandle->ReleaseHandle();
		State->LibraryLoadHandle.Reset();
	}
	if (State->PatternLoadHandle)
	{
		State->PatternLoadHandle->CancelHandle();
		State->PatternLoadHandle->ReleaseHandle();
		State->PatternLoadHandle.Reset();
	}
	if (State->OverrideLoadHandle)
	{
		State->OverrideLoadHandle->CancelHandle();
		State->OverrideLoadHandle->ReleaseHandle();
		State->OverrideLoadHandle.Reset();
	}
	State->ActiveLibraryPreload = {};
	State->LoadingLibraryPaths.Reset();
	State->LoadingLibraries.Reset();
	State->LibraryResolver.Release();
	FOpenMobileHapticsBackendRegistry::GetTimelineManager().Clear();
	if (PreviousPreparationState
		!= EOpenMobileHapticPreparationState::Unprepared)
	{
		if (IOpenMobileHapticsBackend* Backend =
			FOpenMobileHapticsBackendRegistry::FindBackend())
		{
			Backend->ReleasePreparedResources();
		}
	}
	State->LastNamedPatternStatus =
		EOpenMobileHapticNamedPatternStatus::Unprepared;
	State->PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;
	if (bNotifyCancellation && ActiveHandle.IsValid())
	{
		FOpenMobileHapticLibraryPreloadResult Result;
		Result.Handle = ActiveHandle;
		Result.Outcome = EOpenMobileHapticLibraryPreloadOutcome::Cancelled;
		OnNamedLibrariesPrepared.Broadcast(Result);
	}
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::StopPlayback(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return StopPlaybackNative(Handle);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::CancelPlayback(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return CancelPlaybackNative(Handle);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::UpdatePlaybackParameters(
	FOpenMobileHapticPlaybackHandle Handle,
	const FOpenMobileHapticDynamicParameterUpdate& Update
)
{
	return UpdatePlaybackParametersNative(Handle, Update);
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
	return SubmitSemanticOrOverride(Request, NAME_None);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitSemanticOrOverride(
	const FOpenMobileHapticSemanticRequest& Request,
	FName PatternOverride
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
	const float StaticIntensity = FOpenMobileHapticsIntensityPolicy::Scale(
		Request.Intensity,
		1.0f,
		1.0f,
		1.0f,
		Request.Options.IntensityScale,
		ProjectScale
	);
	const float MutablePolicyScale = FOpenMobileHapticsIntensityPolicy::Scale(
		1.0f,
		UserPolicy.MasterIntensity,
		OpenMobileHapticsSubsystemPrivate::FindScale(
			UserPolicy.CategoryScales,
			Request.Options.Category
		),
		OpenMobileHapticsSubsystemPrivate::FindScale(
			UserPolicy.EffectScales,
			Descriptor.Name
		),
		1.0f,
		1.0f
	);
	AdjustedRequest.Intensity = StaticIntensity * MutablePolicyScale;
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
	const FOpenMobileHapticCapabilities Capabilities =
		FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot();
	const FOpenMobileHapticsSemanticResolution Resolution =
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			Request.Options.FallbackPolicy
		);
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

	bool bOverrideFailed = false;
	if (!PatternOverride.IsNone())
	{
		const bool bSupportsDynamicParameters =
			Capabilities.DynamicParameters
				== EOpenMobileHapticSupportState::Supported
			&& Backend->GetControlSupport().bDynamicParameters;
		FOpenMobileHapticNamedPatternRequest NamedRequest;
		NamedRequest.PatternName = PatternOverride;
		NamedRequest.Intensity = bSupportsDynamicParameters
			? StaticIntensity
			: AdjustedRequest.Intensity;
		NamedRequest.Options = AdjustedRequest.Options;
		FOpenMobileHapticsBackendPlaybackParameters PlaybackParameters;
		PlaybackParameters.bHasInitialDynamicParameters =
			bSupportsDynamicParameters;
		PlaybackParameters.InitialDynamicParameters.Intensity =
			MutablePolicyScale;
		const FOpenMobileHapticsBackendRequestToken OverrideToken =
			FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, true);
		FOpenMobileHapticsSubsystemRequestState OverrideState;
		OverrideState.Token = OverrideToken;
		OverrideState.Channel = Request.Options.Channel;
		OverrideState.Category = Request.Options.Category;
		OverrideState.Effect = Descriptor.Name;
		OverrideState.bSupportsDynamicParameters =
			bSupportsDynamicParameters;
		LocalState.Requests.Add(OverrideToken.RequestId, OverrideState);
		FOpenMobileHapticPlaybackResult OverrideResult =
			OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
				LocalState,
				OverrideToken,
				Request.Options.Channel,
				Backend->SubmitNamedPattern(
					NamedRequest,
					PlaybackParameters,
					OverrideToken,
					MakeBackendCallback()
				)
			);
		if (OverrideResult.IsAccepted()
			|| OverrideResult.Outcome
				== EOpenMobileHapticPlaybackOutcome::Suppressed)
		{
			OverrideResult.ResolvedPath = TEXT("NamedLibrary");
			if (OverrideResult.IsAccepted())
			{
				OverrideResult.Intensity.Requested = Request.Intensity;
				OverrideResult.Intensity.Resolved = AdjustedRequest.Intensity;
				LocalState.LastIntensity = OverrideResult.Intensity;
			}
			return OverrideResult;
		}
		if (Request.Options.FallbackPolicy
			== EOpenMobileHapticFallbackPolicy::ExactOnly)
		{
			return OverrideResult;
		}
		bOverrideFailed = true;
	}

	if (Resolution.Path == EOpenMobileHapticsSemanticPath::Unsupported)
	{
		if (Resolution.bSuppressWhenUnavailable)
		{
			return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
				Request.Options.Channel,
				TEXT("Unavailable")
			);
		}
		if (!Backend->IsCustomPlaybackConfigured()
			&& Request.Options.FallbackPolicy
				!= EOpenMobileHapticFallbackPolicy::ExactOnly)
		{
			return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
				EOpenMobileHapticsFailureReason::NotConfigured,
				EOpenMobileHapticFailureStage::Capability,
				Descriptor.Name,
				Request.Options.Channel
			);
		}
		FOpenMobileHapticPlaybackResult Unsupported =
			OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
				EOpenMobileHapticsFailureReason::UnsupportedFeature,
				EOpenMobileHapticFailureStage::Capability,
				Descriptor.Name,
				Request.Options.Channel
			);
		if (bOverrideFailed)
		{
			Unsupported.Error.FallbackAttempts.Add(PatternOverride);
		}
		return Unsupported;
	}
	const FOpenMobileHapticsIntensityResolution IntensityResolution =
		Resolution.Path == EOpenMobileHapticsSemanticPath::BasicVibration
			? FOpenMobileHapticsIntensityPolicy::ResolveBasicVibration(
				AdjustedRequest.Intensity,
				Capabilities.AmplitudeControl,
				Request.Options.FallbackPolicy
			)
			: FOpenMobileHapticsIntensityResolution{
				EOpenMobileHapticsIntensityOutcome::Accepted,
				AdjustedRequest.Intensity
			};
	if (IntensityResolution.Outcome
		== EOpenMobileHapticsIntensityOutcome::Suppressed)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("UnavailableIntensity")
		);
	}
	if (IntensityResolution.Outcome
		== EOpenMobileHapticsIntensityOutcome::Rejected)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::UnsupportedFeature,
			EOpenMobileHapticFailureStage::Capability,
			Descriptor.Name,
			Request.Options.Channel
		);
	}
	const FOpenMobileHapticsBackendRequestToken Token =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, false);
	LocalState.Requests.Add(
		Token.RequestId,
		{Token, 0, Request.Options.Channel}
	);
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
		Result.Intensity.Requested = Request.Intensity;
		Result.Intensity.Resolved = AdjustedRequest.Intensity;
		if (IntensityResolution.Outcome
			== EOpenMobileHapticsIntensityOutcome::DefaultAmplitudeFallback)
		{
			Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
			Result.ResolvedPath = TEXT("BasicVibrationDefaultAmplitude");
			Result.Intensity.bNativeIntensityKnown =
				IntensityResolution.bNativeIntensityKnown;
			Result.Intensity.Native = IntensityResolution.NativeIntensity;
			Result.Intensity.bNativeClamped =
				IntensityResolution.bNativeClamped;
		}
		LocalState.LastIntensity = Result.Intensity;
		if (Result.ResolvedPath.IsNone())
		{
			Result.ResolvedPath =
				FOpenMobileHapticsSemanticPolicy::PathName(Resolution.Path);
		}
		if (Resolution.bFallback || bOverrideFailed)
		{
			Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
		}
		if (bOverrideFailed)
		{
			LocalState.LastError = {};
		}
	}
	else if (bOverrideFailed)
	{
		Result.Error.FallbackAttempts.Add(PatternOverride);
	}
	return Result;
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitOneShot(
	const FOpenMobileHapticOneShotRequest& Request
)
{
	check(IsInGameThread());
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	if (!FMath::IsFinite(Request.DurationSeconds)
		|| Request.DurationSeconds < 0.0f
		|| (Request.DurationSeconds > 0.0f
			&& !FOpenMobileHapticsDurationPolicy::IsWithinBounds(
				Request.DurationSeconds,
				Settings->MinimumOneShotDurationSeconds,
				Settings->MaximumOneShotDurationSeconds
			))
		|| !FMath::IsFinite(Request.Intensity)
		|| Request.Intensity < 0.0f
		|| Request.Intensity > 1.0f
		|| !FMath::IsFinite(Request.Options.IntensityScale)
		|| Request.Options.IntensityScale < 0.0f
		|| Request.Options.IntensityScale > 1.0f
		|| Request.Options.Channel.IsNone()
		|| Request.Options.Category.IsNone()
		|| static_cast<uint8>(Request.Options.OverlapPolicy)
			> static_cast<uint8>(EOpenMobileHapticOverlapPolicy::MixWhenSupported))
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::InvalidRequest,
			EOpenMobileHapticFailureStage::Validation,
			TEXT("OneShot"),
			Request.Options.Channel
		);
	}
	if (Request.DurationSeconds == 0.0f
		|| Request.Intensity == 0.0f
		|| !UserPolicy.bEnabled)
	{
		const FName Reason = !UserPolicy.bEnabled
			? FName(TEXT("PlayerPolicy"))
			: Request.DurationSeconds == 0.0f
				? FName(TEXT("ZeroDuration"))
				: FName(TEXT("ZeroIntensity"));
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			Reason
		);
	}
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

	FOpenMobileHapticOneShotRequest AdjustedRequest = Request;
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
		if (Effect.Name == TEXT("OneShot"))
		{
			ProjectScale *= Effect.IntensityScale;
			MinimumIntervalSeconds = FMath::Max<double>(
				MinimumIntervalSeconds,
				Effect.MinimumIntervalSeconds
			);
			break;
		}
	}
	AdjustedRequest.Intensity = FOpenMobileHapticsIntensityPolicy::Scale(
		Request.Intensity,
		UserPolicy.MasterIntensity,
		OpenMobileHapticsSubsystemPrivate::FindScale(
			UserPolicy.CategoryScales,
			Request.Options.Category
		),
		OpenMobileHapticsSubsystemPrivate::FindScale(
			UserPolicy.EffectScales,
			TEXT("OneShot")
		),
		Request.Options.IntensityScale,
		ProjectScale
	);
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
	const FOpenMobileHapticCapabilities Capabilities =
		FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot();
	const FOpenMobileHapticsOneShotResolution Resolution =
		FOpenMobileHapticsOneShotPolicy::Resolve(
			Capabilities,
			Request.DurationSeconds,
			Request.Options.FallbackPolicy
		);
	if (Resolution.Path == EOpenMobileHapticsOneShotPath::Unsupported)
	{
		if (Resolution.bSuppressWhenUnavailable)
		{
			return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
				Request.Options.Channel,
				TEXT("Unavailable")
			);
		}
		if (!Backend->IsCustomPlaybackConfigured())
		{
			return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
				EOpenMobileHapticsFailureReason::NotConfigured,
				EOpenMobileHapticFailureStage::Capability,
				TEXT("OneShot"),
				Request.Options.Channel
			);
		}
		return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::UnsupportedFeature,
			EOpenMobileHapticFailureStage::Capability,
			TEXT("OneShot"),
			Request.Options.Channel
		);
	}
	if (Resolution.Path == EOpenMobileHapticsOneShotPath::BasicVibration
		&& Capabilities.MaximumDurationSeconds.bKnown
		&& FOpenMobileHapticsDurationPolicy::ResolveNativeLimit(
			Request.DurationSeconds,
			Capabilities.MaximumDurationSeconds.Seconds,
			false
		).Outcome == EOpenMobileHapticsNativeDurationOutcome::Rejected)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::UnsupportedFeature,
			EOpenMobileHapticFailureStage::Capability,
			TEXT("OneShotDuration"),
			Request.Options.Channel
		);
	}
	const FOpenMobileHapticsIntensityResolution IntensityResolution =
		Resolution.Path == EOpenMobileHapticsOneShotPath::BasicVibration
			? FOpenMobileHapticsIntensityPolicy::ResolveBasicVibration(
				AdjustedRequest.Intensity,
				Capabilities.AmplitudeControl,
				Request.Options.FallbackPolicy
			)
			: FOpenMobileHapticsIntensityResolution{
				EOpenMobileHapticsIntensityOutcome::Accepted,
				AdjustedRequest.Intensity
			};
	if (IntensityResolution.Outcome
		== EOpenMobileHapticsIntensityOutcome::Suppressed)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("UnavailableIntensity")
		);
	}
	if (IntensityResolution.Outcome
		== EOpenMobileHapticsIntensityOutcome::Rejected)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::UnsupportedFeature,
			EOpenMobileHapticFailureStage::Capability,
			TEXT("OneShotIntensity"),
			Request.Options.Channel
		);
	}
	if (LocalState.RateLimiter.ShouldSuppress(
		Request.Options.Channel,
		false,
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
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, true);
	LocalState.Requests.Add(
		Token.RequestId,
		{Token, 0, Request.Options.Channel}
	);
	FOpenMobileHapticPlaybackResult Result =
		OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
		LocalState,
		Token,
		Request.Options.Channel,
		Backend->SubmitOneShot(
			AdjustedRequest,
			Resolution,
			Token,
			MakeBackendCallback()
		)
	);
	if (Result.IsAccepted() && Result.ResolvedPath.IsNone())
	{
		Result.ResolvedPath =
			FOpenMobileHapticsOneShotPolicy::PathName(Resolution.Path);
	}
	if (Result.IsAccepted())
	{
		Result.Duration.RequestedSeconds = Request.DurationSeconds;
		Result.Duration.ResolvedSeconds = AdjustedRequest.DurationSeconds;
		Result.Intensity.Requested = Request.Intensity;
		Result.Intensity.Resolved = AdjustedRequest.Intensity;
		if (IntensityResolution.Outcome
			== EOpenMobileHapticsIntensityOutcome::DefaultAmplitudeFallback)
		{
			Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
			Result.ResolvedPath = TEXT("BasicVibrationDefaultAmplitude");
			Result.Intensity.bNativeIntensityKnown =
				IntensityResolution.bNativeIntensityKnown;
			Result.Intensity.Native = IntensityResolution.NativeIntensity;
			Result.Intensity.bNativeClamped =
				IntensityResolution.bNativeClamped;
		}
		LocalState.LastDuration = Result.Duration;
		LocalState.LastIntensity = Result.Intensity;
	}
	return Result;
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitNamedPattern(
	const FOpenMobileHapticNamedPatternRequest& Request
)
{
	check(IsInGameThread());
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	if (Request.PatternName.IsNone()
		|| !FMath::IsFinite(Request.Intensity)
		|| Request.Intensity < 0.0f
		|| Request.Intensity > 1.0f
		|| !FMath::IsFinite(Request.Options.IntensityScale)
		|| Request.Options.IntensityScale < 0.0f
		|| Request.Options.IntensityScale > 1.0f
		|| Request.Options.Channel.IsNone()
		|| Request.Options.Category.IsNone()
		|| static_cast<uint8>(Request.Options.OverlapPolicy)
			> static_cast<uint8>(EOpenMobileHapticOverlapPolicy::MixWhenSupported))
	{
		FOpenMobileHapticPlaybackResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
				EOpenMobileHapticsFailureReason::InvalidRequest,
				EOpenMobileHapticFailureStage::Validation,
				Request.PatternName,
				Request.Options.Channel
			);
		LocalState.LastError = Result.Error;
		return Result;
	}
	if (!UserPolicy.bEnabled)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("PlayerPolicy")
		);
	}
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
	}

	FOpenMobileHapticNamedPatternRequest ResolvedRequest = Request;
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	float ProjectScale = 1.0f;
	for (const FOpenMobileHapticChannelSettings& Channel : Settings->Channels)
	{
		if (Channel.Name == Request.Options.Channel)
		{
			ProjectScale *= Channel.IntensityScale;
			break;
		}
	}
	for (const FOpenMobileHapticEffectSettings& Effect :
		Settings->EffectOverrides)
	{
		if (Effect.Name == Request.PatternName)
		{
			ProjectScale *= Effect.IntensityScale;
			break;
		}
	}
	const float StaticIntensity = FOpenMobileHapticsIntensityPolicy::Scale(
		Request.Intensity,
		1.0f,
		1.0f,
		1.0f,
		Request.Options.IntensityScale,
		ProjectScale
	);
	const float MutablePolicyScale = FOpenMobileHapticsIntensityPolicy::Scale(
		1.0f,
		UserPolicy.MasterIntensity,
		OpenMobileHapticsSubsystemPrivate::FindScale(
			UserPolicy.CategoryScales,
			Request.Options.Category
		),
		OpenMobileHapticsSubsystemPrivate::FindScale(
			UserPolicy.EffectScales,
			Request.PatternName
		),
		1.0f,
		1.0f
	);
	if (StaticIntensity <= 0.0f || MutablePolicyScale <= 0.0f)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("ZeroIntensity")
		);
	}
	if (!Settings->NamedLibraries.IsEmpty())
	{
		LocalState.LastNamedPattern = Request.PatternName;
		LocalState.LastNamedPatternStatus =
			LocalState.LibraryResolver.GetStatus(Request.PatternName);
		if (LocalState.LastNamedPatternStatus
			!= EOpenMobileHapticNamedPatternStatus::Loaded
			|| !LocalState.LibraryResolver.Find(
				Request.PatternName,
				ResolvedRequest.PatternAsset
			))
		{
			const EOpenMobileHapticsFailureReason Reason =
				LocalState.LastNamedPatternStatus
					== EOpenMobileHapticNamedPatternStatus::Missing
				|| LocalState.LastNamedPatternStatus
					== EOpenMobileHapticNamedPatternStatus::Invalid
					? EOpenMobileHapticsFailureReason::InvalidPattern
					: EOpenMobileHapticsFailureReason::NotConfigured;
			FOpenMobileHapticPlaybackResult Result =
				OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
					Reason,
					EOpenMobileHapticFailureStage::Preparation,
					Request.PatternName,
					Request.Options.Channel
				);
			LocalState.LastError = Result.Error;
			return Result;
		}
		const UOpenMobileHapticPatternAsset* Pattern =
			Cast<UOpenMobileHapticPatternAsset>(
				ResolvedRequest.PatternAsset.ResolveObject()
			);
		if (Pattern)
		{
			ResolvedRequest.PlatformOverrideAsset =
				Pattern->GetOverrideForCurrentPlatform();
		}
	}

	const FOpenMobileHapticCapabilities Capabilities =
		FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot();
	if ((Request.Options.Schedule.Mode
				!= EOpenMobileHapticScheduleMode::Immediate
			|| Request.Options.Schedule.LatencyOffsetSeconds > 0.0)
		&& Capabilities.Scheduling
			!= EOpenMobileHapticSupportState::Supported)
	{
		FOpenMobileHapticPlaybackResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
				EOpenMobileHapticsFailureReason::UnsupportedFeature,
				EOpenMobileHapticFailureStage::Capability,
				Request.PatternName,
				Request.Options.Channel
			);
		LocalState.LastError = Result.Error;
		return Result;
	}
	const EOpenMobileHapticSynchronizationMode SynchronizationMode =
		Request.Options.Schedule.Mode
			== EOpenMobileHapticScheduleMode::AbsoluteAudioTime
				? EOpenMobileHapticSynchronizationMode::BestEffort
				: EOpenMobileHapticSynchronizationMode::None;
	const FOpenMobileHapticsTimingResolution Timing =
		LocalState.TimingPolicy.Resolve(
			Request.Options.Schedule,
			FPlatformTime::Seconds(),
			static_cast<int64>(
				FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
			),
			SynchronizationMode
		);
	if (Timing.Outcome != EOpenMobileHapticsTimingOutcome::Ready)
	{
		FOpenMobileHapticPlaybackResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeTimingRejectedPlaybackResult(
				Timing.Outcome,
				Request.PatternName,
				Request.Options.Channel
			);
		LocalState.LastError = Result.Error;
		return Result;
	}
	const bool bSupportsDynamicParameters =
		Capabilities.DynamicParameters
			== EOpenMobileHapticSupportState::Supported
		&& Backend->GetControlSupport().bDynamicParameters;
	ResolvedRequest.Intensity = bSupportsDynamicParameters
		? StaticIntensity
		: StaticIntensity * MutablePolicyScale;
	FOpenMobileHapticsBackendPlaybackParameters PlaybackParameters;
	PlaybackParameters.bHasInitialDynamicParameters =
		bSupportsDynamicParameters;
	PlaybackParameters.InitialDynamicParameters.Intensity =
		MutablePolicyScale;
	PlaybackParameters.Timing = Timing;
	const UOpenMobileHapticPatternAsset* PortablePattern =
		Cast<UOpenMobileHapticPatternAsset>(
			ResolvedRequest.PatternAsset.ResolveObject()
		);
	if (PortablePattern)
	{
		FOpenMobileHapticLoopOptions EffectiveLoop = PortablePattern->Loop;
		if (ResolvedRequest.Options.Loop.bLoop)
		{
			EffectiveLoop = ResolvedRequest.Options.Loop;
		}
		FOpenMobileHapticsTimelineManager& TimelineManager =
			FOpenMobileHapticsBackendRegistry::GetTimelineManager();
		TimelineManager.SetLimits(
			OpenMobileHapticsSubsystemPrivate::PreparedResourceLimits(*Settings)
		);
		PlaybackParameters.PortableTimeline =
			TimelineManager.Resolve(
				Backend->GetBackendName(),
				*PortablePattern,
				EffectiveLoop,
				Capabilities,
				ResolvedRequest.Intensity,
				ResolvedRequest.Options.FallbackPolicy,
				FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
			).Timeline;
	}

	const FOpenMobileHapticsBackendRequestToken Token =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, true);
	FOpenMobileHapticsSubsystemRequestState RequestState;
	RequestState.Token = Token;
	RequestState.Channel = ResolvedRequest.Options.Channel;
	RequestState.Category = ResolvedRequest.Options.Category;
	RequestState.Effect = ResolvedRequest.PatternName;
	RequestState.bSupportsDynamicParameters = bSupportsDynamicParameters;
	LocalState.Requests.Add(Token.RequestId, RequestState);
	FOpenMobileHapticsBackendSubmission Submission =
		Backend->SubmitNamedPattern(
			ResolvedRequest,
			PlaybackParameters,
			Token,
			MakeBackendCallback()
		);
	if (Submission.Result.IsAccepted())
	{
		if (Submission.Result.Synchronization.Mode
			== EOpenMobileHapticSynchronizationMode::None)
		{
			Submission.Result.Synchronization = Timing.Diagnostics;
		}
		if (Timing.StartDelaySeconds > 0.0
			&& Submission.Result.State
				== EOpenMobileHapticPlaybackState::Accepted)
		{
			Submission.Result.State =
				EOpenMobileHapticPlaybackState::Scheduled;
		}
	}
	FOpenMobileHapticPlaybackResult Result =
		OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
			LocalState,
			Token,
			ResolvedRequest.Options.Channel,
			MoveTemp(Submission)
		);
	if (Result.IsAccepted())
	{
		Result.Intensity.Requested = Request.Intensity;
		Result.Intensity.Resolved = StaticIntensity * MutablePolicyScale;
		LocalState.LastIntensity = Result.Intensity;
	}
	return Result;
}

void UOpenMobileHapticsSubsystem::CompleteControlledRequest(
	uint64 RequestId,
	EOpenMobileHapticPlaybackState TerminalState
)
{
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const FOpenMobileHapticsSubsystemRequestState* Request =
		LocalState.Requests.Find(RequestId);
	if (!Request || !Request->Token.PlaybackHandle.IsValid())
	{
		return;
	}
	const FOpenMobileHapticPlaybackHandle Handle =
		Request->Token.PlaybackHandle;
	const FName Channel = Request->Channel;
	LocalState.PlaybackStates.Add(Handle, TerminalState);
	OpenMobileHapticsSubsystemPrivate::RemoveRequest(LocalState, RequestId);

	FOpenMobileHapticPlaybackEvent Event;
	Event.Handle = Handle;
	Event.State = TerminalState;
	Event.Evidence = EOpenMobileHapticEventEvidence::SchedulerConfirmed;
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.Channel = Channel;
	OnPlaybackEvent.Broadcast(Event);
	NativePlaybackEvent.Broadcast(Event);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::EndPlaybackNative(
	FOpenMobileHapticPlaybackHandle Handle,
	EOpenMobileHapticPlaybackState TerminalState
)
{
	check(IsInGameThread());
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const uint64* RequestId = LocalState.RequestByHandle.Find(Handle);
	FOpenMobileHapticsSubsystemRequestState* Request = RequestId
		? LocalState.Requests.Find(*RequestId)
		: nullptr;
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Request)
	{
		const EOpenMobileHapticPlaybackState* ExistingState =
			LocalState.PlaybackStates.Find(Handle);
		if (ExistingState
			&& (*ExistingState == EOpenMobileHapticPlaybackState::Stopped
				|| *ExistingState
					== EOpenMobileHapticPlaybackState::Cancelled))
		{
			FOpenMobileHapticControlResult Result;
			Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
			return Result;
		}
		if (!Backend)
		{
			return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
		}
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
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
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
	const uint64 OwnedRequestId = Request->Token.RequestId;
	FOpenMobileHapticControlResult Result =
		Backend->StopPlayback(Request->Token);
	if (Result.Outcome == EOpenMobileHapticControlOutcome::Accepted)
	{
		CompleteControlledRequest(OwnedRequestId, TerminalState);
	}
	return Result;
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::StopPlaybackNative(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return EndPlaybackNative(
		Handle,
		EOpenMobileHapticPlaybackState::Stopped
	);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::CancelPlaybackNative(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return EndPlaybackNative(
		Handle,
		EOpenMobileHapticPlaybackState::Cancelled
	);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::UpdatePlaybackParametersNative(
	FOpenMobileHapticPlaybackHandle Handle,
	const FOpenMobileHapticDynamicParameterUpdate& Update
)
{
	check(IsInGameThread());
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	if (!FOpenMobileHapticsDynamicParameterPolicy::IsValid(Update))
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = EOpenMobileHapticsFailureReason::InvalidRequest;
		Context.Stage = EOpenMobileHapticFailureStage::Validation;
		Context.Handle = Handle;
		return FOpenMobileHapticControlResult::MakeRejected(
			FOpenMobileHapticsErrorMapper::Map(Context)
		);
	}

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

	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend
		|| !FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(Request->Token)
		|| Backend->GetBackendName() != Request->Token.BackendName)
	{
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
	if (!Request->bSupportsDynamicParameters
		|| !Backend->GetControlSupport().bDynamicParameters)
	{
		FOpenMobileHapticControlResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
		Result.Error.Handle = Handle;
		Result.Error.bRejectedBeforeSubmission = false;
		return Result;
	}

	FOpenMobileHapticDynamicParameterUpdate EffectiveUpdate = Update;
	if (Update.bUpdateIntensity)
	{
		EffectiveUpdate.Intensity = FOpenMobileHapticsIntensityPolicy::Scale(
			Update.Intensity,
			OpenMobileHapticsSubsystemPrivate::ActivePolicyScale(
				UserPolicy,
				*Request
			),
			1.0f,
			1.0f,
			1.0f,
			1.0f
		);
	}
	return QueueDynamicParameterUpdate(
		Request->Token.RequestId,
		EffectiveUpdate,
		&Update
	);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::QueueDynamicParameterUpdate(
	uint64 RequestId,
	const FOpenMobileHapticDynamicParameterUpdate& EffectiveUpdate,
	const FOpenMobileHapticDynamicParameterUpdate* RequestedUpdate
)
{
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	FOpenMobileHapticsSubsystemRequestState* Request =
		LocalState.Requests.Find(RequestId);
	if (!Request)
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
		return Result;
	}
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const double MinimumIntervalSeconds =
		OpenMobileHapticsSubsystemPrivate::DynamicParameterInterval(*Settings);
	const double NowSeconds = FPlatformTime::Seconds();
	FOpenMobileHapticDynamicParameterUpdate Ready;
	const EOpenMobileHapticsDynamicParameterQueueOutcome QueueOutcome =
		LocalState.DynamicParameterPolicy.Queue(
			RequestId,
			EffectiveUpdate,
			NowSeconds,
			MinimumIntervalSeconds,
			Ready
		);
	if (QueueOutcome
		== EOpenMobileHapticsDynamicParameterQueueOutcome::Invalid)
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = EOpenMobileHapticsFailureReason::Internal;
		Context.Stage = EOpenMobileHapticFailureStage::Playback;
		Context.Handle = Request->Token.PlaybackHandle;
		Context.bAfterAcceptance = true;
		return FOpenMobileHapticControlResult::MakeRejected(
			FOpenMobileHapticsErrorMapper::Map(Context)
		);
	}

	FOpenMobileHapticControlResult Result;
	if (QueueOutcome == EOpenMobileHapticsDynamicParameterQueueOutcome::Ready)
	{
		Result = SubmitDynamicParameterUpdate(RequestId, Ready, NowSeconds);
		if (Result.Outcome != EOpenMobileHapticControlOutcome::Accepted)
		{
			return Result;
		}
	}
	else
	{
		Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
		ScheduleDynamicParameterFlush();
	}

	if (RequestedUpdate)
	{
		if (RequestedUpdate->bUpdateIntensity)
		{
			Request->RuntimeIntensity = RequestedUpdate->Intensity;
		}
		if (RequestedUpdate->bUpdateSharpness)
		{
			Request->RuntimeSharpness = RequestedUpdate->Sharpness;
		}
	}
	return Result;
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::SubmitDynamicParameterUpdate(
	uint64 RequestId,
	const FOpenMobileHapticDynamicParameterUpdate& Update,
	double SubmissionTimeSeconds
)
{
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	FOpenMobileHapticsSubsystemRequestState* Request =
		LocalState.Requests.Find(RequestId);
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Request
		|| !Backend
		|| !FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(Request->Token)
		|| Backend->GetBackendName() != Request->Token.BackendName)
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
		return Result;
	}
	if (!Request->bSupportsDynamicParameters
		|| !Backend->GetControlSupport().bDynamicParameters)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
	}

	FOpenMobileHapticControlResult Result =
		Backend->UpdatePlaybackParameters(Request->Token, Update);
	LocalState.DynamicParameterPolicy.MarkAttempted(
		RequestId,
		SubmissionTimeSeconds
	);
	if (Result.Outcome == EOpenMobileHapticControlOutcome::Accepted)
	{
		return Result;
	}

	FOpenMobileHapticsErrorContext Context;
	Context.Reason = Result.Outcome
		== EOpenMobileHapticControlOutcome::Unsupported
			? EOpenMobileHapticsFailureReason::UnsupportedFeature
			: Result.Outcome == EOpenMobileHapticControlOutcome::StaleHandle
				? EOpenMobileHapticsFailureReason::BackendUnavailable
				: EOpenMobileHapticsFailureReason::NativeEngineFailure;
	Context.Stage = EOpenMobileHapticFailureStage::Playback;
	Context.Handle = Request->Token.PlaybackHandle;
	Context.Channel = Request->Channel;
	Context.bAfterAcceptance = true;
	Result.Error = FOpenMobileHapticsErrorMapper::Complete(
		MoveTemp(Result.Error),
		Context
	);
	Result.Error.bRejectedBeforeSubmission = false;
	LocalState.LastError = Result.Error;
	return Result;
}

void UOpenMobileHapticsSubsystem::ScheduleDynamicParameterFlush()
{
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	if (LocalState.DynamicParameterTickerHandle.IsValid() || bDeinitialized)
	{
		return;
	}
	LocalState.DynamicParameterTickerHandle =
		FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UOpenMobileHapticsSubsystem::TickDynamicParameterUpdates
		)
	);
}

void UOpenMobileHapticsSubsystem::FlushDynamicParameterUpdates(
	double NowSeconds
)
{
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const double MinimumIntervalSeconds =
		OpenMobileHapticsSubsystemPrivate::DynamicParameterInterval(*Settings);
	TArray<FOpenMobileHapticsScheduledDynamicParameterUpdate> Updates;
	State->DynamicParameterPolicy.CollectReady(
		NowSeconds,
		MinimumIntervalSeconds,
		Updates
	);
	for (const FOpenMobileHapticsScheduledDynamicParameterUpdate& Update :
		Updates)
	{
		SubmitDynamicParameterUpdate(
			Update.RequestId,
			Update.Update,
			NowSeconds
		);
	}
}

bool UOpenMobileHapticsSubsystem::TickDynamicParameterUpdates(
	float DeltaTime
)
{
	static_cast<void>(DeltaTime);
	FlushDynamicParameterUpdates(FPlatformTime::Seconds());
	if (State && State->DynamicParameterPolicy.HasPending())
	{
		return true;
	}
	if (State)
	{
		State->DynamicParameterTickerHandle.Reset();
	}
	return false;
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
	if (Channel.IsNone())
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = EOpenMobileHapticsFailureReason::InvalidRequest;
		Context.Stage = EOpenMobileHapticFailureStage::Validation;
		return FOpenMobileHapticControlResult::MakeRejected(
			FOpenMobileHapticsErrorMapper::Map(Context)
		);
	}
	FOpenMobileHapticControlResult Result = Backend->StopChannel(Channel);
	if (Result.Outcome == EOpenMobileHapticControlOutcome::Accepted)
	{
		FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
		TArray<uint64> RequestIds;
		for (const TPair<uint64, FOpenMobileHapticsSubsystemRequestState>& Pair :
			LocalState.Requests)
		{
			if (Pair.Value.Channel == Channel
				&& Pair.Value.Token.PlaybackHandle.IsValid())
			{
				RequestIds.Add(Pair.Key);
			}
		}
		for (const uint64 RequestId : RequestIds)
		{
			CompleteControlledRequest(
				RequestId,
				EOpenMobileHapticPlaybackState::Stopped
			);
		}
	}
	return Result;
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
	FOpenMobileHapticControlResult Result = Backend->StopAll();
	if (Result.Outcome == EOpenMobileHapticControlOutcome::Accepted)
	{
		FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
		TArray<uint64> RequestIds;
		for (const TPair<uint64, FOpenMobileHapticsSubsystemRequestState>& Pair :
			LocalState.Requests)
		{
			if (Pair.Value.Token.PlaybackHandle.IsValid())
			{
				RequestIds.Add(Pair.Key);
			}
		}
		for (const uint64 RequestId : RequestIds)
		{
			CompleteControlledRequest(
				RequestId,
				EOpenMobileHapticPlaybackState::Stopped
			);
		}
	}
	return Result;
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
	check(IsInGameThread());
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
	if (State)
	{
		TArray<uint64> RequestIds;
		State->Requests.GetKeys(RequestIds);
		for (const uint64 RequestId : RequestIds)
		{
			const FOpenMobileHapticsSubsystemRequestState* Request =
				State->Requests.Find(RequestId);
			if (!Request || !Request->bSupportsDynamicParameters)
			{
				continue;
			}
			FOpenMobileHapticDynamicParameterUpdate Update;
			Update.Intensity = FOpenMobileHapticsIntensityPolicy::Scale(
				Request->RuntimeIntensity,
				OpenMobileHapticsSubsystemPrivate::ActivePolicyScale(
					Policy,
					*Request
				),
				1.0f,
				1.0f,
				1.0f,
				1.0f
			);
			QueueDynamicParameterUpdate(RequestId, Update, nullptr);
		}
	}
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
		Diagnostics.LastDuration = State->LastDuration;
		Diagnostics.LastIntensity = State->LastIntensity;
		Diagnostics.LastNamedPattern = State->LastNamedPattern;
		Diagnostics.LastNamedPatternStatus = State->LastNamedPatternStatus;
		Diagnostics.PreparedNamedPatternCount =
			State->LibraryResolver.GetPreparedPatternCount();
		Diagnostics.PreparationState = GetPreparationState();
		Diagnostics.LastResolvedPath = State->LastResolvedPath;
		Diagnostics.LastFallbackAttempts = State->LastFallbackAttempts;
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

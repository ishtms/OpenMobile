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
#include "OpenMobileHapticsLifecyclePolicy.h"
#include "OpenMobileHapticsOneShotPolicy.h"
#include "OpenMobileHapticsPlaybackControlPolicy.h"
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
	FName ResolvedPath;
	TArray<FName> FallbackAttempts;
	EOpenMobileHapticPlaybackState LastPublishedState =
		EOpenMobileHapticPlaybackState::Invalid;
	EOpenMobileHapticPlaybackState SubmissionState =
		EOpenMobileHapticPlaybackState::Invalid;
	double LastEventTimestampSeconds = 0.0;
	double SubmissionTimestampSeconds = 0.0;
	double EstimatedStartTimeSeconds = 0.0;
	float RuntimeIntensity = 1.0f;
	float RuntimeSharpness = 0.5f;
	bool bSupportsDynamicParameters = false;
	bool bRequiresPreparedAsset = false;
	TOptional<FOpenMobileHapticNamedPatternRequest> RecoveryRequest;
	FOpenMobileHapticPlaybackHandle RecoverySourceHandle;
	FOpenMobileHapticsBackendPlaybackControlSupport PlaybackControlSupport;
	TOptional<FOpenMobileHapticsPlaybackControlPolicy> PlaybackControlPolicy;
	TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> ScheduledStartGuard;
	FTSTicker::FDelegateHandle EstimatedStartTickerHandle;
	FTSTicker::FDelegateHandle TerminalWatchdogTickerHandle;
};

struct FOpenMobileHapticsPendingRecoveryPlayback
{
	FOpenMobileHapticNamedPatternRequest Request;
	FOpenMobileHapticPlaybackHandle SourceHandle;
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
	TArray<FOpenMobileHapticPlaybackEvent> RecentPlaybackEvents;
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
	TArray<FOpenMobileHapticsPendingRecoveryPlayback> PendingRecoveryPlaybacks;
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

	FOpenMobileHapticPlaybackResult MakeRecoveryPendingPlaybackResult(
		FName Effect,
		FName Channel
	)
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = EOpenMobileHapticsFailureReason::BackendUnavailable;
		Context.Stage = EOpenMobileHapticFailureStage::Interruption;
		Context.FailedItem = Effect;
		Context.Channel = Channel;
		FOpenMobileHapticPlaybackResult Result =
			FOpenMobileHapticPlaybackResult::MakeRejected(
				FOpenMobileHapticsErrorMapper::Map(Context)
			);
		Result.Error.Message = TEXT(
			"The Haptics backend is recovering from an interruption."
		);
		return Result;
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

	EOpenMobileHapticsLifecycleRequestOutcome EvaluateLifecycle(
		const FOpenMobileHapticPlaybackOptions& Options,
		EOpenMobileHapticsLifecycleRequestKind Kind,
		EOpenMobileHapticSemanticEffect SemanticEffect =
			EOpenMobileHapticSemanticEffect::Selection,
		bool bPatternSuitableForBackgroundPlayback = false
	)
	{
		FOpenMobileHapticsLifecycleRequestContext Context;
		Context.BackgroundPolicy =
			GetDefault<UOpenMobileHapticsSettings>()->BackgroundPolicy;
		Context.BackgroundAlerts =
			FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot()
				.BackgroundAlerts;
		Context.Priority = Options.Priority;
		Context.Category = Options.Category;
		Context.Kind = Kind;
		Context.SemanticEffect = SemanticEffect;
		Context.bPatternSuitableForBackgroundPlayback =
			bPatternSuitableForBackgroundPlayback;
		return FOpenMobileHapticsLifecyclePolicy::Evaluate(
			Context,
			FOpenMobileHapticsBackendRegistry::GetApplicationState()
		);
	}

	FName LifecycleSuppressionReason()
	{
		switch (FOpenMobileHapticsBackendRegistry::GetApplicationState())
		{
		case EOpenMobileHapticsApplicationState::Inactive:
			return TEXT("ApplicationInactive");
		case EOpenMobileHapticsApplicationState::Terminating:
			return TEXT("ApplicationTerminating");
		default:
			return TEXT("BackgroundPolicy");
		}
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

	bool ResolvePlaybackTiming(
		FOpenMobileHapticsSubsystemState& State,
		const FOpenMobileHapticCapabilities& Capabilities,
		const FOpenMobileHapticSchedule& Schedule,
		FName Effect,
		FName Channel,
		FOpenMobileHapticsTimingResolution& OutTiming,
		FOpenMobileHapticPlaybackResult& OutRejection
	)
	{
		if ((Schedule.Mode != EOpenMobileHapticScheduleMode::Immediate
				|| Schedule.LatencyOffsetSeconds > 0.0)
			&& Capabilities.Scheduling
				!= EOpenMobileHapticSupportState::Supported)
		{
			OutRejection = MakeRejectedPlaybackResult(
				EOpenMobileHapticsFailureReason::UnsupportedFeature,
				EOpenMobileHapticFailureStage::Capability,
				Effect,
				Channel
			);
			return false;
		}
		const EOpenMobileHapticSynchronizationMode SynchronizationMode =
			Schedule.Mode == EOpenMobileHapticScheduleMode::AbsoluteAudioTime
				? EOpenMobileHapticSynchronizationMode::BestEffort
				: EOpenMobileHapticSynchronizationMode::None;
		OutTiming = State.TimingPolicy.Resolve(
			Schedule,
			FPlatformTime::Seconds(),
			static_cast<int64>(
				FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
			),
			SynchronizationMode
		);
		if (OutTiming.Outcome == EOpenMobileHapticsTimingOutcome::Ready)
		{
			return true;
		}
		OutRejection = MakeTimingRejectedPlaybackResult(
			OutTiming.Outcome,
			Effect,
			Channel
		);
		return false;
	}

	void ApplyResolvedTiming(
		FOpenMobileHapticPlaybackResult& Result,
		const FOpenMobileHapticsTimingResolution& Timing
	)
	{
		if (!Result.IsAccepted())
		{
			return;
		}
		if (Result.Synchronization.Mode
			== EOpenMobileHapticSynchronizationMode::None)
		{
			Result.Synchronization = Timing.Diagnostics;
		}
		if (Timing.StartDelaySeconds > 0.0
			&& Result.State == EOpenMobileHapticPlaybackState::Accepted)
		{
			Result.State = EOpenMobileHapticPlaybackState::Scheduled;
		}
	}

	TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> MakeScheduledStartGuard(
		const FOpenMobileHapticsTimingResolution& Timing
	)
	{
		if (Timing.StartDelaySeconds <= 0.0)
		{
			return nullptr;
		}
		return MakeShared<
			FOpenMobileHapticsScheduledStartGuard,
			ESPMode::ThreadSafe
		>(FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration());
	}

	void InvalidateScheduledStarts(
		FOpenMobileHapticsSubsystemState& State,
		bool bPreparedAssetsOnly = false
	)
	{
		for (TPair<uint64, FOpenMobileHapticsSubsystemRequestState>& Request :
			State.Requests)
		{
			if (Request.Value.ScheduledStartGuard
				&& (!bPreparedAssetsOnly
					|| Request.Value.bRequiresPreparedAsset))
			{
				Request.Value.ScheduledStartGuard->Invalidate();
			}
		}
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

	bool CanPublishState(
		EOpenMobileHapticPlaybackState CurrentState,
		EOpenMobileHapticPlaybackState NextState
	)
	{
		if (CurrentState == EOpenMobileHapticPlaybackState::Invalid)
		{
			return NextState == EOpenMobileHapticPlaybackState::Accepted;
		}
		if (IsTerminalState(CurrentState)
			|| NextState == EOpenMobileHapticPlaybackState::Invalid
			|| NextState == EOpenMobileHapticPlaybackState::Accepted)
		{
			return false;
		}
		if (IsTerminalState(NextState))
		{
			return true;
		}
		switch (NextState)
		{
		case EOpenMobileHapticPlaybackState::Scheduled:
			return CurrentState == EOpenMobileHapticPlaybackState::Accepted;
		case EOpenMobileHapticPlaybackState::Started:
			return CurrentState == EOpenMobileHapticPlaybackState::Accepted
				|| CurrentState == EOpenMobileHapticPlaybackState::Scheduled;
		case EOpenMobileHapticPlaybackState::Paused:
			return CurrentState == EOpenMobileHapticPlaybackState::Started
				|| CurrentState == EOpenMobileHapticPlaybackState::Resumed;
		case EOpenMobileHapticPlaybackState::Resumed:
			return CurrentState == EOpenMobileHapticPlaybackState::Paused;
		default:
			return false;
		}
	}

	bool IsValidEvidence(EOpenMobileHapticEventEvidence Evidence)
	{
		return Evidence == EOpenMobileHapticEventEvidence::Estimated
			|| Evidence
				== EOpenMobileHapticEventEvidence::SchedulerConfirmed
			|| Evidence == EOpenMobileHapticEventEvidence::NativeConfirmed;
	}

	FName SanitizeHistoryName(FName Value)
	{
		if (Value.IsNone())
		{
			return NAME_None;
		}
		const FString Text = Value.ToString();
		if (Text.Len() > 64
			|| Text.Contains(TEXT("/"))
			|| Text.Contains(TEXT("\\"))
			|| Text.Contains(TEXT("\n"))
			|| Text.Contains(TEXT("\r")))
		{
			return TEXT("redacted");
		}
		return Value;
	}

	FString SanitizeHistoryText(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return {};
		}
		if (Value.Contains(TEXT("/"))
			|| Value.Contains(TEXT("\\"))
			|| Value.Contains(TEXT("\n"))
			|| Value.Contains(TEXT("\r")))
		{
			return TEXT("redacted");
		}
		return Value.Left(256);
	}

	FOpenMobileHapticPlaybackEvent SanitizeHistoryEvent(
		const FOpenMobileHapticPlaybackEvent& Event
	)
	{
		FOpenMobileHapticPlaybackEvent Sanitized = Event;
		Sanitized.PatternOrEffect = SanitizeHistoryName(Event.PatternOrEffect);
		Sanitized.Channel = SanitizeHistoryName(Event.Channel);
		Sanitized.ResolvedPath = SanitizeHistoryName(Event.ResolvedPath);
		Sanitized.Error.Message = SanitizeHistoryText(Event.Error.Message);
		Sanitized.Error.NativeDomain =
			SanitizeHistoryText(Event.Error.NativeDomain).Left(64);
		Sanitized.Error.NativeCode =
			SanitizeHistoryText(Event.Error.NativeCode).Left(64);
		Sanitized.Error.FailedItem =
			SanitizeHistoryName(Event.Error.FailedItem);
		Sanitized.Error.Channel = SanitizeHistoryName(Event.Error.Channel);
		for (FName& Attempt : Sanitized.Error.FallbackAttempts)
		{
			Attempt = SanitizeHistoryName(Attempt);
		}
		Sanitized.Error.Correction =
			SanitizeHistoryText(Event.Error.Correction);
		return Sanitized;
	}

	void AppendEventHistory(
		FOpenMobileHapticsSubsystemState& State,
		const FOpenMobileHapticPlaybackEvent& Event
	)
	{
		const int32 MaximumEvents = FMath::Clamp(
			GetDefault<UOpenMobileHapticsSettings>()->MaximumDiagnosticEvents,
			1,
			512
		);
		const int32 Excess = State.RecentPlaybackEvents.Num()
			- MaximumEvents + 1;
		if (Excess > 0)
		{
			State.RecentPlaybackEvents.RemoveAt(
				0,
				Excess,
				EAllowShrinking::No
			);
		}
		State.RecentPlaybackEvents.Add(SanitizeHistoryEvent(Event));
	}

	void RemoveRequest(
		FOpenMobileHapticsSubsystemState& State,
		uint64 RequestId
	)
	{
		if (const FOpenMobileHapticsSubsystemRequestState* Request =
			State.Requests.Find(RequestId))
		{
			if (Request->EstimatedStartTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(
					Request->EstimatedStartTickerHandle
				);
			}
			if (Request->TerminalWatchdogTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(
					Request->TerminalWatchdogTickerHandle
				);
			}
			if (Request->ScheduledStartGuard)
			{
				Request->ScheduledStartGuard->Invalidate();
			}
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
			FOpenMobileHapticsSubsystemRequestState* Request =
				State.Requests.Find(Token.RequestId);
			if (Request)
			{
				Request->PlaybackControlSupport =
					Submission.PlaybackControlSupport;
				if (Submission.PlaybackControlSupport.bHasRepeatPlan
					&& Submission.PlaybackControlSupport.SupportsAnyControl())
				{
					Request->PlaybackControlPolicy.Emplace(
						Submission.PlaybackControlSupport.RepeatPlan,
						Result.State,
						FPlatformTime::Seconds()
					);
				}
			}
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
	State->RecentPlaybackEvents.Reserve(FMath::Clamp(
		Settings->MaximumDiagnosticEvents,
		1,
		512
	));
	BindRecoveryEvents();
}

void UOpenMobileHapticsSubsystem::Deinitialize()
{
	if (bDeinitialized)
	{
		return;
	}
	UnbindRecoveryEvents();
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
		if (State)
		{
			OpenMobileHapticsSubsystemPrivate::InvalidateScheduledStarts(*State);
		}
		if (State && Backend->GetControlSupport().bStop)
		{
			TArray<FOpenMobileHapticsBackendRequestToken> OwnedTokens;
			for (const TPair<
				uint64,
				FOpenMobileHapticsSubsystemRequestState
			>& Pair : State->Requests)
			{
				if (Pair.Value.Token.PlaybackHandle.IsValid()
					&& Pair.Value.Token.BackendName
						== Backend->GetBackendName())
				{
					OwnedTokens.Add(Pair.Value.Token);
				}
			}
			OwnedTokens.Sort([](
				const FOpenMobileHapticsBackendRequestToken& Left,
				const FOpenMobileHapticsBackendRequestToken& Right
			)
			{
				return Left.RequestId < Right.RequestId;
			});
			for (const FOpenMobileHapticsBackendRequestToken& Token : OwnedTokens)
			{
				Backend->StopPlayback(Token);
			}
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
	if (State)
	{
		TArray<uint64> RequestIds;
		State->Requests.GetKeys(RequestIds);
		for (const uint64 RequestId : RequestIds)
		{
			OpenMobileHapticsSubsystemPrivate::RemoveRequest(*State, RequestId);
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
	if (!FOpenMobileHapticsBackendRegistry::RequestRecovery(
		UserPolicy.bEnabled
	))
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
	TArray<FString>& Errors,
	bool bPreserveResolvedLibrariesOnNativeFailure
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
		if (bPreserveResolvedLibrariesOnNativeFailure)
		{
			State->PreparationState =
				EOpenMobileHapticPreparationState::Prepared;
		}
		else
		{
			State->LibraryResolver.FailPreparation();
			State->PreparationState =
				EOpenMobileHapticPreparationState::Failed;
		}
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
	OpenMobileHapticsSubsystemPrivate::InvalidateScheduledStarts(*State, true);
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

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::PausePlayback(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return PausePlaybackNative(Handle);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::ResumePlayback(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return ResumePlaybackNative(Handle);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::SeekPlayback(
	FOpenMobileHapticPlaybackHandle Handle,
	double PositionSeconds
)
{
	return SeekPlaybackNative(Handle, PositionSeconds);
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
		|| Request.Options.Category.IsNone()
		|| static_cast<uint8>(Request.Options.InterruptionPolicy)
			> static_cast<uint8>(
				EOpenMobileHapticInterruptionPolicy::Restart
			))
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
	const EOpenMobileHapticsLifecycleRequestOutcome LifecycleOutcome =
		OpenMobileHapticsSubsystemPrivate::EvaluateLifecycle(
			Request.Options,
			EOpenMobileHapticsLifecycleRequestKind::Semantic,
			Request.Effect
		);
	if (LifecycleOutcome
		== EOpenMobileHapticsLifecycleRequestOutcome::Suppressed)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			OpenMobileHapticsSubsystemPrivate::LifecycleSuppressionReason()
		);
	}
	if (LifecycleOutcome
		== EOpenMobileHapticsLifecycleRequestOutcome::BackgroundAlert)
	{
		PatternOverride = NAME_None;
	}
	if (!FOpenMobileHapticsBackendRegistry::RequestRecovery(
		UserPolicy.bEnabled
	))
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRecoveryPendingPlaybackResult(
			Descriptor.Name,
			Request.Options.Channel
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
	FOpenMobileHapticCapabilities Capabilities =
		FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot();
	if (LifecycleOutcome
		== EOpenMobileHapticsLifecycleRequestOutcome::BackgroundAlert)
	{
		Capabilities.SemanticEffects =
			EOpenMobileHapticSupportState::Unsupported;
	}
	FOpenMobileHapticsTimingResolution Timing;
	FOpenMobileHapticPlaybackResult TimingRejection;
	if (!OpenMobileHapticsSubsystemPrivate::ResolvePlaybackTiming(
		LocalState,
		Capabilities,
		Request.Options.Schedule,
		Descriptor.Name,
		Request.Options.Channel,
		Timing,
		TimingRejection
	))
	{
		LocalState.LastError = TimingRejection.Error;
		return TimingRejection;
	}
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
		PlaybackParameters.Timing = Timing;
		PlaybackParameters.ScheduledStartGuard =
			OpenMobileHapticsSubsystemPrivate::MakeScheduledStartGuard(Timing);
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
		OverrideState.bRequiresPreparedAsset = true;
		OverrideState.ScheduledStartGuard =
			PlaybackParameters.ScheduledStartGuard;
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
		OpenMobileHapticsSubsystemPrivate::ApplyResolvedTiming(
			OverrideResult,
			Timing
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
			PublishSubmissionEvents(OverrideResult, Timing);
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
	FOpenMobileHapticsBackendPlaybackParameters PlaybackParameters;
	PlaybackParameters.Timing = Timing;
	PlaybackParameters.ScheduledStartGuard =
		OpenMobileHapticsSubsystemPrivate::MakeScheduledStartGuard(Timing);
	const bool bScheduled = PlaybackParameters.ScheduledStartGuard.IsValid();
	const FOpenMobileHapticsBackendRequestToken Token =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(
			*Backend,
			bScheduled
		);
	FOpenMobileHapticsSubsystemRequestState RequestState;
	RequestState.Token = Token;
	RequestState.Channel = Request.Options.Channel;
	RequestState.Category = Request.Options.Category;
	RequestState.Effect = Descriptor.Name;
	RequestState.ScheduledStartGuard =
		PlaybackParameters.ScheduledStartGuard;
	LocalState.Requests.Add(Token.RequestId, MoveTemp(RequestState));
	FOpenMobileHapticPlaybackResult Result =
		OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
			LocalState,
		Token,
		Request.Options.Channel,
			Backend->SubmitSemantic(
				AdjustedRequest,
				Resolution,
				PlaybackParameters,
				Token,
				MakeBackendCallback()
			)
		);
	OpenMobileHapticsSubsystemPrivate::ApplyResolvedTiming(Result, Timing);
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
	PublishSubmissionEvents(Result, Timing);
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
			> static_cast<uint8>(EOpenMobileHapticOverlapPolicy::MixWhenSupported)
		|| static_cast<uint8>(Request.Options.InterruptionPolicy)
			> static_cast<uint8>(
				EOpenMobileHapticInterruptionPolicy::Restart
			))
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
	if (OpenMobileHapticsSubsystemPrivate::EvaluateLifecycle(
		Request.Options,
		EOpenMobileHapticsLifecycleRequestKind::OneShot
	) == EOpenMobileHapticsLifecycleRequestOutcome::Suppressed)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			OpenMobileHapticsSubsystemPrivate::LifecycleSuppressionReason()
		);
	}
	if (!FOpenMobileHapticsBackendRegistry::RequestRecovery(
		UserPolicy.bEnabled
	))
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRecoveryPendingPlaybackResult(
			TEXT("OneShot"),
			Request.Options.Channel
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
	FOpenMobileHapticsTimingResolution Timing;
	FOpenMobileHapticPlaybackResult TimingRejection;
	if (!OpenMobileHapticsSubsystemPrivate::ResolvePlaybackTiming(
		LocalState,
		Capabilities,
		Request.Options.Schedule,
		TEXT("OneShot"),
		Request.Options.Channel,
		Timing,
		TimingRejection
	))
	{
		LocalState.LastError = TimingRejection.Error;
		return TimingRejection;
	}
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
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(
			*Backend,
			true
		);
	FOpenMobileHapticsBackendPlaybackParameters PlaybackParameters;
	PlaybackParameters.Timing = Timing;
	PlaybackParameters.ScheduledStartGuard =
		OpenMobileHapticsSubsystemPrivate::MakeScheduledStartGuard(Timing);
	FOpenMobileHapticsSubsystemRequestState RequestState;
	RequestState.Token = Token;
	RequestState.Channel = Request.Options.Channel;
	RequestState.Category = Request.Options.Category;
	RequestState.Effect = TEXT("OneShot");
	RequestState.ScheduledStartGuard =
		PlaybackParameters.ScheduledStartGuard;
	LocalState.Requests.Add(Token.RequestId, MoveTemp(RequestState));
	FOpenMobileHapticPlaybackResult Result =
		OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
		LocalState,
		Token,
		Request.Options.Channel,
		Backend->SubmitOneShot(
			AdjustedRequest,
			Resolution,
			PlaybackParameters,
			Token,
			MakeBackendCallback()
		)
	);
	OpenMobileHapticsSubsystemPrivate::ApplyResolvedTiming(Result, Timing);
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
	PublishSubmissionEvents(Result, Timing);
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
			> static_cast<uint8>(EOpenMobileHapticOverlapPolicy::MixWhenSupported)
		|| static_cast<uint8>(Request.Options.InterruptionPolicy)
			> static_cast<uint8>(
				EOpenMobileHapticInterruptionPolicy::Restart
			))
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
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	FSoftObjectPath LifecyclePatternPath = Request.PatternAsset;
	if (Settings->NamedLibraries.Num() > 0
		&& !LocalState.LibraryResolver.Find(
			Request.PatternName,
			LifecyclePatternPath
		))
	{
		LifecyclePatternPath.Reset();
	}
	const UOpenMobileHapticPatternAsset* LifecyclePattern =
		Cast<UOpenMobileHapticPatternAsset>(
			LifecyclePatternPath.ResolveObject()
		);
	if (OpenMobileHapticsSubsystemPrivate::EvaluateLifecycle(
		Request.Options,
		EOpenMobileHapticsLifecycleRequestKind::NamedPattern,
		EOpenMobileHapticSemanticEffect::Selection,
		LifecyclePattern
			&& LifecyclePattern->bSuitableForBackgroundPlayback
	) == EOpenMobileHapticsLifecycleRequestOutcome::Suppressed)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			OpenMobileHapticsSubsystemPrivate::LifecycleSuppressionReason()
		);
	}
	if (!FOpenMobileHapticsBackendRegistry::RequestRecovery(
		UserPolicy.bEnabled
	))
	{
		FOpenMobileHapticPlaybackResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeRecoveryPendingPlaybackResult(
				Request.PatternName,
				Request.Options.Channel
			);
		LocalState.LastError = Result.Error;
		return Result;
	}
	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
	}

	FOpenMobileHapticNamedPatternRequest ResolvedRequest = Request;
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
	if (!Settings->NamedLibraries.IsEmpty()
		&& LocalState.PreparationState
			== EOpenMobileHapticPreparationState::Prepared
		&& Backend->GetPreparationState()
			!= EOpenMobileHapticPreparationState::Prepared)
	{
		TArray<FString> PreparationErrors;
		if (!PrepareResolvedResources(PreparationErrors, true))
		{
			FOpenMobileHapticPlaybackResult Result =
				OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
					EOpenMobileHapticsFailureReason::NativeEngineFailure,
					EOpenMobileHapticFailureStage::Preparation,
					Request.PatternName,
					Request.Options.Channel
				);
			if (!PreparationErrors.IsEmpty())
			{
				Result.Error.Message = PreparationErrors[0].Left(256);
			}
			LocalState.LastError = Result.Error;
			return Result;
		}
	}

	const FOpenMobileHapticCapabilities Capabilities =
		FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot();
	FOpenMobileHapticsTimingResolution Timing;
	FOpenMobileHapticPlaybackResult TimingRejection;
	if (!OpenMobileHapticsSubsystemPrivate::ResolvePlaybackTiming(
		LocalState,
		Capabilities,
		Request.Options.Schedule,
		Request.PatternName,
		Request.Options.Channel,
		Timing,
		TimingRejection
	))
	{
		LocalState.LastError = TimingRejection.Error;
		return TimingRejection;
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
	PlaybackParameters.ScheduledStartGuard =
		OpenMobileHapticsSubsystemPrivate::MakeScheduledStartGuard(Timing);
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
	RequestState.bRequiresPreparedAsset = !Settings->NamedLibraries.IsEmpty();
	RequestState.RecoveryRequest = Request;
	RequestState.ScheduledStartGuard =
		PlaybackParameters.ScheduledStartGuard;
	LocalState.Requests.Add(Token.RequestId, RequestState);
	FOpenMobileHapticsBackendSubmission Submission =
		Backend->SubmitNamedPattern(
			ResolvedRequest,
			PlaybackParameters,
			Token,
			MakeBackendCallback()
		);
	OpenMobileHapticsSubsystemPrivate::ApplyResolvedTiming(
		Submission.Result,
		Timing
	);
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
	PublishSubmissionEvents(Result, Timing);
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
	FOpenMobileHapticPlaybackEvent Event;
	Event.State = TerminalState;
	Event.Evidence = EOpenMobileHapticEventEvidence::SchedulerConfirmed;
	Event.TimestampSeconds = FPlatformTime::Seconds();
	PublishPlaybackEvent(RequestId, MoveTemp(Event));
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
	if (!Backend
		|| !FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(Request->Token)
		|| Backend->GetBackendName() != Request->Token.BackendName)
	{
		const uint64 OwnedRequestId = Request->Token.RequestId;
		CompleteControlledRequest(
			OwnedRequestId,
			EOpenMobileHapticPlaybackState::Interrupted
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
	if (Request->ScheduledStartGuard)
	{
		Request->ScheduledStartGuard->Invalidate();
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
UOpenMobileHapticsSubsystem::PausePlaybackNative(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return ApplyPlaybackCursorControl(
		Handle,
		EPlaybackCursorControl::Pause,
		0.0
	);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::ResumePlaybackNative(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return ApplyPlaybackCursorControl(
		Handle,
		EPlaybackCursorControl::Resume,
		0.0
	);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::SeekPlaybackNative(
	FOpenMobileHapticPlaybackHandle Handle,
	double PositionSeconds
)
{
	return ApplyPlaybackCursorControl(
		Handle,
		EPlaybackCursorControl::Seek,
		PositionSeconds
	);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::ApplyPlaybackCursorControl(
	FOpenMobileHapticPlaybackHandle Handle,
	EPlaybackCursorControl Control,
	double PositionSeconds
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
		const uint64 OwnedRequestId = Request->Token.RequestId;
		CompleteControlledRequest(
			OwnedRequestId,
			EOpenMobileHapticPlaybackState::Interrupted
		);
		return Result;
	}

	const FOpenMobileHapticsBackendControlSupport BackendSupport =
		Backend->GetControlSupport();
	const EOpenMobileHapticControlImplementation Implementation =
		Control == EPlaybackCursorControl::Pause
			? Request->PlaybackControlSupport.PauseImplementation
			: Control == EPlaybackCursorControl::Resume
				? Request->PlaybackControlSupport.ResumeImplementation
				: Request->PlaybackControlSupport.SeekImplementation;
	const bool bBackendSupportsControl =
		Control == EPlaybackCursorControl::Pause
			? BackendSupport.bPause
			: Control == EPlaybackCursorControl::Resume
				? BackendSupport.bResume
				: BackendSupport.bSeek;
	if (!bBackendSupportsControl
		|| (Implementation != EOpenMobileHapticControlImplementation::Native
			&& Implementation
				!= EOpenMobileHapticControlImplementation::Emulated)
		|| !Request->PlaybackControlPolicy.IsSet()
		|| !Request->PlaybackControlPolicy->IsValid())
	{
		FOpenMobileHapticControlResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeUnsupportedControlResult();
		Result.Implementation =
			EOpenMobileHapticControlImplementation::Unsupported;
		Result.Error.Handle = Handle;
		Result.Error.bRejectedBeforeSubmission = false;
		return Result;
	}

	FOpenMobileHapticsPlaybackControlPolicy Candidate =
		Request->PlaybackControlPolicy.GetValue();
	const double NowSeconds = FPlatformTime::Seconds();
	const FOpenMobileHapticsPlaybackControlTransition Transition =
		Control == EPlaybackCursorControl::Pause
			? Candidate.Pause(NowSeconds)
			: Control == EPlaybackCursorControl::Resume
				? Candidate.Resume(NowSeconds)
				: Candidate.Seek(
					PositionSeconds,
					NowSeconds,
					Request->PlaybackControlSupport.SeekGranularitySeconds
				);
	if (Transition.Outcome
		!= EOpenMobileHapticsPlaybackControlTransitionOutcome::Accepted)
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = EOpenMobileHapticsFailureReason::InvalidRequest;
		Context.Stage = EOpenMobileHapticFailureStage::Playback;
		Context.Handle = Handle;
		Context.bAfterAcceptance = true;
		return FOpenMobileHapticControlResult::MakeRejected(
			FOpenMobileHapticsErrorMapper::Map(Context)
		);
	}

	FOpenMobileHapticsBackendControlCommand Command;
	Command.Revision = Transition.Revision;
	Command.State = Transition.State;
	Command.RequestedPositionSeconds = Transition.RequestedPositionSeconds;
	Command.ResolvedPositionSeconds = Transition.ResolvedPositionSeconds;
	Command.ActiveDurationSeconds = Transition.ActiveDurationSeconds;
	Command.PositionGranularitySeconds =
		Request->PlaybackControlSupport.SeekGranularitySeconds;
	Command.CompletedRepeatCount = Transition.CompletedRepeatCount;
	Command.bQuantized = Transition.bQuantized;
	FOpenMobileHapticControlResult Result =
		Control == EPlaybackCursorControl::Pause
			? Backend->PausePlayback(Request->Token, Command)
			: Control == EPlaybackCursorControl::Resume
				? Backend->ResumePlayback(Request->Token, Command)
				: Backend->SeekPlayback(Request->Token, Command);
	if (Result.Outcome != EOpenMobileHapticControlOutcome::Accepted)
	{
		Result.Error.Handle = Handle;
		Result.Error.bRejectedBeforeSubmission = false;
		return Result;
	}

	Request->PlaybackControlPolicy = MoveTemp(Candidate);
	Result.Implementation = Result.Implementation
		== EOpenMobileHapticControlImplementation::None
			? Implementation
			: Result.Implementation;
	Result.State = Transition.State;
	Result.RequestedPositionSeconds = Transition.RequestedPositionSeconds;
	Result.ResolvedPositionSeconds = Transition.ResolvedPositionSeconds;
	Result.PositionGranularitySeconds =
		Request->PlaybackControlSupport.SeekGranularitySeconds;
	Result.CompletedRepeatCount = Transition.CompletedRepeatCount;
	Result.ControlRevision = static_cast<int64>(FMath::Min<uint64>(
		Transition.Revision,
		static_cast<uint64>(MAX_int64)
	));
	Result.bQuantized = Transition.bQuantized;
	if (Control != EPlaybackCursorControl::Seek)
	{
		FOpenMobileHapticPlaybackEvent Event;
		Event.State = Transition.State;
		Event.Evidence = Result.Implementation
			== EOpenMobileHapticControlImplementation::Native
				? EOpenMobileHapticEventEvidence::NativeConfirmed
				: EOpenMobileHapticEventEvidence::SchedulerConfirmed;
		Event.TimestampSeconds = NowSeconds;
		PublishPlaybackEvent(*RequestId, MoveTemp(Event));
	}
	else
	{
		LocalState.PlaybackStates.Add(Handle, Transition.State);
	}
	return Result;
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
		OpenMobileHapticsSubsystemPrivate::InvalidateScheduledStarts(*State);
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
		const int32 MaximumEvents = FMath::Clamp(
			GetDefault<UOpenMobileHapticsSettings>()->MaximumDiagnosticEvents,
			1,
			512
		);
		const int32 FirstEvent = FMath::Max(
			0,
			State->RecentPlaybackEvents.Num() - MaximumEvents
		);
		const int32 EventCount =
			State->RecentPlaybackEvents.Num() - FirstEvent;
		if (EventCount > 0)
		{
			Diagnostics.RecentPlaybackEvents.Append(
				State->RecentPlaybackEvents.GetData() + FirstEvent,
				EventCount
			);
		}
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

void UOpenMobileHapticsSubsystem::BindRecoveryEvents()
{
	if (bDeinitialized)
	{
		return;
	}
	if (!InterruptionDelegateHandle.IsValid())
	{
		InterruptionDelegateHandle =
			FOpenMobileHapticsBackendRegistry::OnInterruption().AddUObject(
				this,
				&UOpenMobileHapticsSubsystem::HandleInterruption
			);
	}
	if (!RecoveryDelegateHandle.IsValid())
	{
		RecoveryDelegateHandle =
			FOpenMobileHapticsBackendRegistry::OnRecovery().AddUObject(
				this,
				&UOpenMobileHapticsSubsystem::HandleRecovery
			);
	}
	if (!ApplicationLifecycleDelegateHandle.IsValid())
	{
		ApplicationLifecycleDelegateHandle =
			FOpenMobileHapticsBackendRegistry::OnApplicationLifecycle()
				.AddUObject(
					this,
					&UOpenMobileHapticsSubsystem::HandleApplicationLifecycle
				);
	}
}

void UOpenMobileHapticsSubsystem::UnbindRecoveryEvents()
{
	if (InterruptionDelegateHandle.IsValid())
	{
		FOpenMobileHapticsBackendRegistry::OnInterruption().Remove(
			InterruptionDelegateHandle
		);
		InterruptionDelegateHandle.Reset();
	}
	if (RecoveryDelegateHandle.IsValid())
	{
		FOpenMobileHapticsBackendRegistry::OnRecovery().Remove(
			RecoveryDelegateHandle
		);
		RecoveryDelegateHandle.Reset();
	}
	if (ApplicationLifecycleDelegateHandle.IsValid())
	{
		FOpenMobileHapticsBackendRegistry::OnApplicationLifecycle().Remove(
			ApplicationLifecycleDelegateHandle
		);
		ApplicationLifecycleDelegateHandle.Reset();
	}
}

void UOpenMobileHapticsSubsystem::HandleInterruption(
	const FOpenMobileHapticsInterruption& Interruption
)
{
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	TArray<uint64> RequestIds;
	for (const TPair<uint64, FOpenMobileHapticsSubsystemRequestState>& Pair :
		State->Requests)
	{
		if (Pair.Value.Token.BackendName == Interruption.BackendName)
		{
			RequestIds.Add(Pair.Key);
		}
	}
	RequestIds.Sort();
	for (const uint64 RequestId : RequestIds)
	{
		if (bDeinitialized || !State)
		{
			return;
		}
		FOpenMobileHapticsSubsystemRequestState* Request =
			State->Requests.Find(RequestId);
		if (!Request)
		{
			continue;
		}
		const bool bCanRestart = Request->RecoveryRequest.IsSet()
			&& Request->RecoveryRequest->Options.InterruptionPolicy
				== EOpenMobileHapticInterruptionPolicy::Restart
			&& Settings->bResumeEligiblePlaybackAfterForeground
			&& UserPolicy.bEnabled
			&& FOpenMobileHapticsBackendRegistry::IsApplicationActive()
			&& (Request->LastPublishedState
					== EOpenMobileHapticPlaybackState::Started
				|| Request->LastPublishedState
					== EOpenMobileHapticPlaybackState::Resumed);
		if (bCanRestart)
		{
			FOpenMobileHapticsPendingRecoveryPlayback Pending;
			Pending.Request = Request->RecoveryRequest.GetValue();
			Pending.Request.Options.Schedule = {};
			Pending.SourceHandle = Request->Token.PlaybackHandle;
			State->PendingRecoveryPlaybacks.Add(MoveTemp(Pending));
		}

		FOpenMobileHapticPlaybackEvent Event;
		Event.State = EOpenMobileHapticPlaybackState::Interrupted;
		Event.Evidence = EOpenMobileHapticEventEvidence::NativeConfirmed;
		Event.TimestampSeconds = FPlatformTime::Seconds();
		PublishPlaybackEvent(RequestId, MoveTemp(Event));
	}

	if (State
		&& State->PreparationState
			== EOpenMobileHapticPreparationState::Preparing)
	{
		ReleaseNamedLibrariesInternal(true);
	}
	if (State && !State->PendingRecoveryPlaybacks.IsEmpty()
		&& UserPolicy.bEnabled)
	{
		FOpenMobileHapticsBackendRegistry::RequestRecovery(true);
	}
}

void UOpenMobileHapticsSubsystem::HandleRecovery()
{
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	TArray<FOpenMobileHapticsPendingRecoveryPlayback> Pending =
		MoveTemp(State->PendingRecoveryPlaybacks);
	State->PendingRecoveryPlaybacks.Reset();
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	if (!UserPolicy.bEnabled
		|| !Settings->bResumeEligiblePlaybackAfterForeground
		|| !FOpenMobileHapticsBackendRegistry::IsApplicationActive())
	{
		return;
	}
	for (FOpenMobileHapticsPendingRecoveryPlayback& Restart : Pending)
	{
		const FOpenMobileHapticPlaybackResult Result =
			SubmitNamedPattern(Restart.Request);
		if (!Result.IsAccepted() || !Result.Handle.IsValid() || !State)
		{
			continue;
		}
		const uint64* RequestId = State->RequestByHandle.Find(Result.Handle);
		FOpenMobileHapticsSubsystemRequestState* Request = RequestId
			? State->Requests.Find(*RequestId)
			: nullptr;
		if (Request)
		{
			Request->RecoverySourceHandle = Restart.SourceHandle;
		}
	}
}

void UOpenMobileHapticsSubsystem::HandleApplicationLifecycle(
	const FOpenMobileHapticsLifecycleTransition& Transition
)
{
	check(IsInGameThread());
	if (bDeinitialized || !State || !Transition.bInterruptsPlayback)
	{
		return;
	}
	State->PendingRecoveryPlaybacks.Reset();
	TArray<uint64> RequestIds;
	State->Requests.GetKeys(RequestIds);
	RequestIds.Sort();
	for (const uint64 RequestId : RequestIds)
	{
		if (bDeinitialized || !State)
		{
			return;
		}
		if (!State->Requests.Contains(RequestId))
		{
			continue;
		}
		FOpenMobileHapticPlaybackEvent Event;
		Event.State = EOpenMobileHapticPlaybackState::Interrupted;
		Event.Evidence = EOpenMobileHapticEventEvidence::NativeConfirmed;
		Event.TimestampSeconds = FPlatformTime::Seconds();
		PublishPlaybackEvent(RequestId, MoveTemp(Event));
	}
	if (State
		&& State->PreparationState
			== EOpenMobileHapticPreparationState::Preparing)
	{
		ReleaseNamedLibrariesInternal(true);
	}
}

FOpenMobileHapticsSubsystemState&
UOpenMobileHapticsSubsystem::GetOrCreateState() const
{
	if (!bDeinitialized)
	{
		const_cast<UOpenMobileHapticsSubsystem*>(this)->BindRecoveryEvents();
	}
	if (!State)
	{
		State.Reset(new FOpenMobileHapticsSubsystemState());
	}
	return *State;
}

void UOpenMobileHapticsSubsystem::PublishSubmissionEvents(
	const FOpenMobileHapticPlaybackResult& Result,
	const FOpenMobileHapticsTimingResolution& Timing
)
{
	check(IsInGameThread());
	if (!Result.IsAccepted() || !Result.Handle.IsValid() || bDeinitialized)
	{
		return;
	}

	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const uint64* RequestId = LocalState.RequestByHandle.Find(Result.Handle);
	FOpenMobileHapticsSubsystemRequestState* Request = RequestId
		? LocalState.Requests.Find(*RequestId)
		: nullptr;
	if (!Request)
	{
		return;
	}
	const uint64 OwnedRequestId = *RequestId;
	Request->ResolvedPath = Result.ResolvedPath;
	Request->FallbackAttempts = Result.FallbackAttempts;
	const double NowSeconds = FPlatformTime::Seconds();
	Request->SubmissionState = Result.State;
	Request->SubmissionTimestampSeconds = NowSeconds;
	Request->EstimatedStartTimeSeconds = NowSeconds
		+ FMath::Max(0.0, Timing.StartDelaySeconds);
	double MaximumPlaybackDurationSeconds = FMath::Clamp<double>(
		GetDefault<UOpenMobileHapticsSettings>()
			->MaximumContinuousDurationSeconds,
		0.1,
		300.0
	);
	if (FMath::IsFinite(Result.Duration.ResolvedSeconds)
		&& Result.Duration.ResolvedSeconds > 0.0)
	{
		MaximumPlaybackDurationSeconds = Result.Duration.ResolvedSeconds;
	}
	else if (Request->PlaybackControlSupport.bHasRepeatPlan)
	{
		const FOpenMobileHapticsRepeatPlan& RepeatPlan =
			Request->PlaybackControlSupport.RepeatPlan;
		const double PlannedDurationSeconds = RepeatPlan.bRepeatUntilStopped
			? RepeatPlan.MaximumDurationSeconds
			: RepeatPlan.TotalDurationSeconds;
		if (FMath::IsFinite(PlannedDurationSeconds)
			&& PlannedDurationSeconds > 0.0)
		{
			MaximumPlaybackDurationSeconds = PlannedDurationSeconds;
		}
	}
	MaximumPlaybackDurationSeconds = FMath::Clamp(
		MaximumPlaybackDurationSeconds,
		0.001,
		300.0
	);

	const TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakSubsystem(this);
	AsyncTask(
		ENamedThreads::GameThread,
		[WeakSubsystem, OwnedRequestId]()
		{
			if (UOpenMobileHapticsSubsystem* Subsystem = WeakSubsystem.Get())
			{
				Subsystem->PublishDeferredSubmissionEvents(OwnedRequestId);
			}
		}
	);
	ScheduleEstimatedStart(OwnedRequestId, Timing.StartDelaySeconds);
	ScheduleTerminalWatchdog(
		OwnedRequestId,
		Timing.StartDelaySeconds + MaximumPlaybackDurationSeconds + 1.0
	);
}

void UOpenMobileHapticsSubsystem::PublishDeferredSubmissionEvents(
	uint64 RequestId
)
{
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	FOpenMobileHapticsSubsystemRequestState* Request =
		State->Requests.Find(RequestId);
	if (!Request
		|| Request->SubmissionState
			== EOpenMobileHapticPlaybackState::Invalid)
	{
		return;
	}
	const EOpenMobileHapticPlaybackState SubmissionState =
		Request->SubmissionState;
	const double SubmissionTimestampSeconds =
		Request->SubmissionTimestampSeconds;
	if (Request->LastPublishedState
		== EOpenMobileHapticPlaybackState::Invalid)
	{
		FOpenMobileHapticPlaybackEvent Event;
		Event.State = EOpenMobileHapticPlaybackState::Accepted;
		Event.Evidence = EOpenMobileHapticEventEvidence::SchedulerConfirmed;
		Event.TimestampSeconds = SubmissionTimestampSeconds;
		PublishPlaybackEvent(RequestId, MoveTemp(Event));
	}

	Request = State ? State->Requests.Find(RequestId) : nullptr;
	if (Request
		&& SubmissionState == EOpenMobileHapticPlaybackState::Scheduled
		&& Request->LastPublishedState
			== EOpenMobileHapticPlaybackState::Accepted)
	{
		FOpenMobileHapticPlaybackEvent Event;
		Event.State = EOpenMobileHapticPlaybackState::Scheduled;
		Event.Evidence = EOpenMobileHapticEventEvidence::SchedulerConfirmed;
		Event.TimestampSeconds = SubmissionTimestampSeconds;
		PublishPlaybackEvent(RequestId, MoveTemp(Event));
	}
}

void UOpenMobileHapticsSubsystem::ScheduleEstimatedStart(
	uint64 RequestId,
	double DelaySeconds
)
{
	check(IsInGameThread());
	if (bDeinitialized || RequestId == 0)
	{
		return;
	}
	FOpenMobileHapticsSubsystemRequestState* Request =
		GetOrCreateState().Requests.Find(RequestId);
	if (!Request || Request->EstimatedStartTickerHandle.IsValid())
	{
		return;
	}
	const TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakSubsystem(this);
	Request->EstimatedStartTickerHandle =
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda(
				[WeakSubsystem, RequestId](float)
				{
					if (UOpenMobileHapticsSubsystem* Subsystem =
						WeakSubsystem.Get())
					{
						Subsystem->PublishEstimatedStart(RequestId);
					}
					return false;
				}
			),
			static_cast<float>(FMath::Clamp(DelaySeconds, 0.0, 60.0))
		);
}

void UOpenMobileHapticsSubsystem::PublishEstimatedStart(uint64 RequestId)
{
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	FOpenMobileHapticsSubsystemRequestState* Request =
		State->Requests.Find(RequestId);
	if (!Request)
	{
		return;
	}
	Request->EstimatedStartTickerHandle.Reset();
	FOpenMobileHapticPlaybackEvent Event;
	Event.State = EOpenMobileHapticPlaybackState::Started;
	Event.Evidence = EOpenMobileHapticEventEvidence::Estimated;
	Event.TimestampSeconds = Request->EstimatedStartTimeSeconds;
	PublishPlaybackEvent(RequestId, MoveTemp(Event));
}

void UOpenMobileHapticsSubsystem::ScheduleTerminalWatchdog(
	uint64 RequestId,
	double DelaySeconds
)
{
	check(IsInGameThread());
	if (bDeinitialized || RequestId == 0)
	{
		return;
	}
	FOpenMobileHapticsSubsystemRequestState* Request =
		GetOrCreateState().Requests.Find(RequestId);
	if (!Request || Request->TerminalWatchdogTickerHandle.IsValid())
	{
		return;
	}
	const TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakSubsystem(this);
	Request->TerminalWatchdogTickerHandle =
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda(
				[WeakSubsystem, RequestId](float)
				{
					if (UOpenMobileHapticsSubsystem* Subsystem =
						WeakSubsystem.Get())
					{
						Subsystem->PublishTerminalTimeout(RequestId);
					}
					return false;
				}
			),
			static_cast<float>(FMath::Clamp(DelaySeconds, 1.0, 361.0))
		);
}

void UOpenMobileHapticsSubsystem::PublishTerminalTimeout(uint64 RequestId)
{
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	FOpenMobileHapticsSubsystemRequestState* Request =
		State->Requests.Find(RequestId);
	if (!Request)
	{
		return;
	}
	Request->TerminalWatchdogTickerHandle.Reset();
	if (Request->LastPublishedState
		== EOpenMobileHapticPlaybackState::Invalid)
	{
		PublishDeferredSubmissionEvents(RequestId);
		Request = State ? State->Requests.Find(RequestId) : nullptr;
		if (!Request)
		{
			return;
		}
	}
	if (Request->LastPublishedState
			== EOpenMobileHapticPlaybackState::Accepted
		|| Request->LastPublishedState
			== EOpenMobileHapticPlaybackState::Scheduled)
	{
		if (Request->EstimatedStartTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(
				Request->EstimatedStartTickerHandle
			);
			Request->EstimatedStartTickerHandle.Reset();
		}
		FOpenMobileHapticPlaybackEvent StartEvent;
		StartEvent.State = EOpenMobileHapticPlaybackState::Started;
		StartEvent.Evidence = EOpenMobileHapticEventEvidence::Estimated;
		StartEvent.TimestampSeconds = FMath::Max(
			Request->EstimatedStartTimeSeconds,
			FPlatformTime::Seconds()
		);
		PublishPlaybackEvent(RequestId, MoveTemp(StartEvent));
		Request = State ? State->Requests.Find(RequestId) : nullptr;
		if (!Request)
		{
			return;
		}
	}
	FOpenMobileHapticPlaybackEvent Event;
	Event.State = EOpenMobileHapticPlaybackState::Failed;
	Event.Evidence = EOpenMobileHapticEventEvidence::Estimated;
	Event.TimestampSeconds = FPlatformTime::Seconds();
	PublishPlaybackEvent(RequestId, MoveTemp(Event));
}

void UOpenMobileHapticsSubsystem::PublishPlaybackEvent(
	uint64 RequestId,
	FOpenMobileHapticPlaybackEvent Event
)
{
	check(IsInGameThread());
	if (bDeinitialized || !State || RequestId == 0)
	{
		return;
	}
	FOpenMobileHapticsSubsystemRequestState* Request =
		State->Requests.Find(RequestId);
	if (!Request || !Request->Token.PlaybackHandle.IsValid())
	{
		return;
	}
	if (Event.State != EOpenMobileHapticPlaybackState::Accepted
		&& Request->LastPublishedState
			== EOpenMobileHapticPlaybackState::Invalid)
	{
		PublishDeferredSubmissionEvents(RequestId);
		Request = State ? State->Requests.Find(RequestId) : nullptr;
		if (!Request)
		{
			return;
		}
	}

	const bool bNeedsEstimatedStart =
		(Event.State == EOpenMobileHapticPlaybackState::Completed
			|| Event.State == EOpenMobileHapticPlaybackState::Paused)
		&& (Request->LastPublishedState
				== EOpenMobileHapticPlaybackState::Accepted
			|| Request->LastPublishedState
				== EOpenMobileHapticPlaybackState::Scheduled);
	if (bNeedsEstimatedStart)
	{
		FOpenMobileHapticPlaybackEvent StartEvent;
		StartEvent.State = EOpenMobileHapticPlaybackState::Started;
		StartEvent.Evidence = EOpenMobileHapticEventEvidence::Estimated;
		StartEvent.TimestampSeconds = Request->EstimatedStartTimeSeconds;
		if (FMath::IsFinite(Event.TimestampSeconds)
			&& Event.TimestampSeconds > 0.0)
		{
			StartEvent.TimestampSeconds = FMath::Min(
				StartEvent.TimestampSeconds,
				Event.TimestampSeconds
			);
		}
		PublishPlaybackEvent(RequestId, MoveTemp(StartEvent));
		Request = State ? State->Requests.Find(RequestId) : nullptr;
		if (!Request)
		{
			return;
		}
	}

	if (!OpenMobileHapticsSubsystemPrivate::CanPublishState(
		Request->LastPublishedState,
		Event.State
	))
	{
		return;
	}
	Event.Handle = Request->Token.PlaybackHandle;
	Event.RecoverySourceHandle = Request->RecoverySourceHandle;
	Event.PatternOrEffect = Request->Effect;
	Event.Channel = Request->Channel;
	Event.ResolvedPath = Request->ResolvedPath;
	if (!OpenMobileHapticsSubsystemPrivate::IsValidEvidence(Event.Evidence))
	{
		Event.Evidence = EOpenMobileHapticEventEvidence::Estimated;
	}
	if (Event.State == EOpenMobileHapticPlaybackState::Accepted
		|| Event.State == EOpenMobileHapticPlaybackState::Scheduled)
	{
		Event.Evidence = EOpenMobileHapticEventEvidence::SchedulerConfirmed;
	}
	if (!FMath::IsFinite(Event.TimestampSeconds)
		|| Event.TimestampSeconds <= 0.0)
	{
		Event.TimestampSeconds = FPlatformTime::Seconds();
	}
	Event.TimestampSeconds = FMath::Max(
		Request->LastEventTimestampSeconds,
		Event.TimestampSeconds
	);

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
		Context.FailedItem = Request->Effect;
		Context.Channel = Request->Channel;
		Context.Handle = Request->Token.PlaybackHandle;
		Context.FallbackAttempts = Request->FallbackAttempts;
		Context.bAfterAcceptance = true;
		Event.Error = FOpenMobileHapticsErrorMapper::Complete(
			MoveTemp(Event.Error),
			Context
		);
		Event.Error.FailedItem = Request->Effect;
		Event.Error.Channel = Request->Channel;
		Event.Error.Handle = Request->Token.PlaybackHandle;
	}

	Request->LastPublishedState = Event.State;
	Request->LastEventTimestampSeconds = Event.TimestampSeconds;
	State->PlaybackStates.Add(Event.Handle, Event.State);
	if (Event.Error.IsSet())
	{
		State->LastError = Event.Error;
	}
	OpenMobileHapticsSubsystemPrivate::AppendEventHistory(*State, Event);
	if (OpenMobileHapticsSubsystemPrivate::IsTerminalState(Event.State))
	{
		OpenMobileHapticsSubsystemPrivate::RemoveRequest(*State, RequestId);
	}

	OnPlaybackEvent.Broadcast(Event);
	NativePlaybackEvent.Broadcast(Event);
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
	PublishPlaybackEvent(Callback.Token.RequestId, Callback.Event);
}

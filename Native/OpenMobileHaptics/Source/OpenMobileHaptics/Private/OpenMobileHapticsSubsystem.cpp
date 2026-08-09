#include "OpenMobileHapticsSubsystem.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobileHapticsBackend.h"
#include "OpenMobileHapticLibrary.h"
#include "OpenMobileHapticNamedPlaybackAsyncAction.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPreparationAsyncAction.h"
#include "OpenMobileHapticPreparationLease.h"
#include "OpenMobileHapticPlayback.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsBudgetPolicy.h"
#include "OpenMobileHapticsChannelPolicy.h"
#include "OpenMobileHapticsDurationPolicy.h"
#include "OpenMobileHapticsDynamicParameterPolicy.h"
#include "OpenMobileHapticsErrorMapper.h"
#include "OpenMobileHapticsIntensityPolicy.h"
#include "OpenMobileHapticsLibraryResolver.h"
#include "OpenMobileHapticsLifecyclePolicy.h"
#include "OpenMobileHapticsNativeEventDispatcher.h"
#include "OpenMobileHapticsOneShotPolicy.h"
#include "OpenMobileHapticsOverlapPolicy.h"
#include "OpenMobileHapticsPerformanceTracker.h"
#include "OpenMobileHapticsPlaybackControlPolicy.h"
#include "OpenMobileHapticsRateLimiter.h"
#include "OpenMobileHapticsSemanticPolicy.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsTimingPolicy.h"
#include "OpenMobileHapticsTimelineManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

struct FOpenMobileHapticsSubsystemRequestState
{
	FOpenMobileHapticsBackendRequestToken Token;
	uint64 LastCallbackSequence = 0;
	FName Channel;
	FName Category;
	FName Effect;
	FName ResolvedPath;
	TArray<FName> FallbackAttempts;
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;
	EOpenMobileHapticPlaybackState LastPublishedState =
		EOpenMobileHapticPlaybackState::Invalid;
	EOpenMobileHapticPlaybackState SubmissionState =
		EOpenMobileHapticPlaybackState::Invalid;
	double LastEventTimestampSeconds = 0.0;
	double SubmissionTimestampSeconds = 0.0;
	double EstimatedStartTimeSeconds = 0.0;
	float RuntimeIntensity = 1.0f;
	float RuntimeSharpness = 0.5f;
	float ResolvedUserPolicyScale = 1.0f;
	bool bSupportsDynamicParameters = false;
	bool bRequiresPreparedAsset = false;
	bool bWaitingForOverlap = false;
	bool bPromotedFromOverlapQueue = false;
	bool bFireAndForgetAfterOverlapPromotion = false;
	double OverlapEnqueuedAtSeconds = 0.0;
	TOptional<FOpenMobileHapticSemanticRequest> QueuedSemanticRequest;
	TOptional<FOpenMobileHapticOneShotRequest> QueuedOneShotRequest;
	TOptional<FOpenMobileHapticNamedPatternRequest> QueuedNamedRequest;
	FName QueuedSemanticPatternOverride;
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
	FTSTicker::FDelegateHandle OverlapQueueExpiryTickerHandle;
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
	uint64 FallbackPlaybackCount = 0;
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
	FOpenMobileHapticsPerformanceTracker PerformanceTracker;
	FOpenMobileHapticsDynamicParameterPolicy DynamicParameterPolicy;
	TArray<FOpenMobileHapticsScheduledDynamicParameterUpdate>
		DynamicParameterBatch;
	FOpenMobileHapticsTimingPolicy TimingPolicy;
	FOpenMobileHapticsChannelArbiter ChannelArbiter;
	TArray<FOpenMobileHapticsPendingRecoveryPlayback> PendingRecoveryPlaybacks;
	TSharedPtr<
		FOpenMobileHapticsNativeEventDispatcher,
		ESPMode::ThreadSafe
	> NativeEventDispatcher;
	FTSTicker::FDelegateHandle DynamicParameterTickerHandle;
	double ActivePreparationStartTimeSeconds = -1.0;
	bool bOverlapQueueDrainScheduled = false;
};

void FOpenMobileHapticsSubsystemStateDeleter::operator()(
	FOpenMobileHapticsSubsystemState* State
) const
{
	delete State;
}

namespace OpenMobileHapticsSubsystemPrivate
{
	constexpr int32 MaximumDiagnosticChannelCount = 64;
	constexpr int32 MaximumDiagnosticHandleCount = 64;

	FName ApplicationStateName(EOpenMobileHapticsApplicationState State)
	{
		switch (State)
		{
		case EOpenMobileHapticsApplicationState::Active:
			return TEXT("Active");
		case EOpenMobileHapticsApplicationState::Inactive:
			return TEXT("Inactive");
		case EOpenMobileHapticsApplicationState::Background:
			return TEXT("Background");
		case EOpenMobileHapticsApplicationState::Terminating:
			return TEXT("Terminating");
		default:
			return TEXT("Unknown");
		}
	}

#if !UE_BUILD_SHIPPING
	bool IsCapabilityTestPlatformOverrideSafe(
		const FSoftObjectPath& OverridePath
	)
	{
		if (OverridePath.IsNull())
		{
			return true;
		}
		UObject* LoadedOverride = OverridePath.TryLoad();
		if (!LoadedOverride)
		{
			return true;
		}
		TArray<FString> Errors;
		if (const UOpenMobileHapticIOSPatternAsset* IOS =
			Cast<UOpenMobileHapticIOSPatternAsset>(LoadedOverride))
		{
			return IOS->Validate(Errors)
				&& FMath::IsFinite(IOS->GetAHAPDurationSeconds())
				&& IOS->GetAHAPDurationSeconds() <= 0.5;
		}
		const UOpenMobileHapticAndroidPatternAsset* Android =
			Cast<UOpenMobileHapticAndroidPatternAsset>(LoadedOverride);
		if (!Android || !Android->Validate(Errors))
		{
			return false;
		}
		switch (Android->Format)
		{
		case EOpenMobileHapticAndroidPatternFormat::Primitives:
		{
			if (Android->Primitives.Num() > 4)
			{
				return false;
			}
			int64 DelayMilliseconds = 0;
			for (const FOpenMobileHapticAndroidPrimitiveStep& Step
				: Android->Primitives)
			{
				DelayMilliseconds += Step.DelayMilliseconds;
			}
			return DelayMilliseconds <= 500;
		}
		case EOpenMobileHapticAndroidPatternFormat::Waveform:
		{
			if (Android->WaveformRepeatIndex != -1)
			{
				return false;
			}
			int64 DurationMilliseconds = 0;
			for (const FOpenMobileHapticAndroidWaveformStep& Step
				: Android->WaveformSteps)
			{
				DurationMilliseconds += Step.DurationMilliseconds;
			}
			return DurationMilliseconds <= 500;
		}
		case EOpenMobileHapticAndroidPatternFormat::BasicEnvelope:
		case EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope:
			return !Android->EnvelopePoints.IsEmpty()
				&& Android->EnvelopePoints.Last().TimeSeconds <= 0.5;
		default:
			return false;
		}
	}
#endif

	int64 ToPublicCounter(uint64 Value)
	{
		return static_cast<int64>(FMath::Min<uint64>(
			Value,
			static_cast<uint64>(MAX_int64)
		));
	}

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
		if (Reason == TEXT("PlayerPolicy"))
		{
			Result.SuppressionReason =
				EOpenMobileHapticSuppressionReason::PlayerPolicy;
		}
		else if (Reason == TEXT("ApplicationInactive")
			|| Reason == TEXT("ApplicationTerminating")
			|| Reason == TEXT("BackgroundPolicy"))
		{
			Result.SuppressionReason =
				EOpenMobileHapticSuppressionReason::Lifecycle;
		}
		else if (Reason == TEXT("ZeroDuration")
			|| Reason == TEXT("ZeroIntensity"))
		{
			Result.SuppressionReason =
				EOpenMobileHapticSuppressionReason::ZeroOutput;
		}
		else if (Reason == TEXT("Unavailable")
			|| Reason == TEXT("UnavailableIntensity"))
		{
			Result.SuppressionReason =
				EOpenMobileHapticSuppressionReason::Unavailable;
		}
		else if (Reason == TEXT("OverlapIgnore")
			|| Reason == TEXT("OverlapPriority"))
		{
			Result.SuppressionReason =
				EOpenMobileHapticSuppressionReason::OverlapPolicy;
		}
		else
		{
			Result.SuppressionReason =
				EOpenMobileHapticSuppressionReason::Other;
		}
		return Result;
	}

	FOpenMobileHapticPlaybackResult MakeRateLimitedPlaybackResult(
		FName Channel,
		EOpenMobileHapticsRateLimitOutcome Outcome
	)
	{
		FName Path = TEXT("RateLimited");
		EOpenMobileHapticSuppressionReason Reason =
			EOpenMobileHapticSuppressionReason::Other;
		switch (Outcome)
		{
		case EOpenMobileHapticsRateLimitOutcome::EquivalentRequest:
			Path = TEXT("CoalescedEquivalentRequest");
			Reason = EOpenMobileHapticSuppressionReason::EquivalentRequest;
			break;
		case EOpenMobileHapticsRateLimitOutcome::ChannelMinimumInterval:
			Path = TEXT("ChannelMinimumInterval");
			Reason =
				EOpenMobileHapticSuppressionReason::ChannelMinimumInterval;
			break;
		case EOpenMobileHapticsRateLimitOutcome::EffectMinimumInterval:
			Path = TEXT("EffectMinimumInterval");
			Reason =
				EOpenMobileHapticSuppressionReason::EffectMinimumInterval;
			break;
		case EOpenMobileHapticsRateLimitOutcome::ChannelWindow:
			Path = TEXT("ChannelRateLimit");
			Reason = EOpenMobileHapticSuppressionReason::ChannelWindow;
			break;
		case EOpenMobileHapticsRateLimitOutcome::GlobalWindow:
			Path = TEXT("GlobalRateLimit");
			Reason = EOpenMobileHapticSuppressionReason::GlobalWindow;
			break;
		case EOpenMobileHapticsRateLimitOutcome::InvalidClock:
			Path = TEXT("RateLimitClockInvalid");
			Reason = EOpenMobileHapticSuppressionReason::InvalidClock;
			break;
		default:
			break;
		}
		FOpenMobileHapticPlaybackResult Result =
			MakeSuppressedPlaybackResult(Channel, Path);
		Result.SuppressionReason = Reason;
		return Result;
	}

	FOpenMobileHapticsRateLimitPolicy ResolveRateLimitPolicy(
		const UOpenMobileHapticsSettings& Settings,
		FName ChannelName,
		FName EffectName,
		double EquivalentRequestDebounceSeconds
	)
	{
		FOpenMobileHapticsRateLimitPolicy Policy;
		Policy.ChannelMinimumIntervalSeconds =
			Settings.DefaultMinimumIntervalSeconds;
		Policy.MaximumChannelSubmissionsPerSecond =
			Settings.MaximumSubmissionsPerSecond;
		Policy.MaximumGlobalSubmissionsPerSecond =
			Settings.MaximumSubmissionsPerSecond;
		Policy.EquivalentRequestDebounceSeconds =
			EquivalentRequestDebounceSeconds;
		for (const FOpenMobileHapticChannelSettings& Channel :
			Settings.Channels)
		{
			if (Channel.Name == ChannelName)
			{
				Policy.ChannelMinimumIntervalSeconds =
					Channel.MinimumIntervalSeconds;
				Policy.MaximumChannelSubmissionsPerSecond =
					Channel.MaximumSubmissionsPerSecond;
				break;
			}
		}
		for (const FOpenMobileHapticEffectSettings& Effect :
			Settings.EffectOverrides)
		{
			if (Effect.Name == EffectName)
			{
				Policy.EffectMinimumIntervalSeconds =
					Effect.MinimumIntervalSeconds;
				break;
			}
		}
		return Policy;
	}

	FOpenMobileHapticsRateLimitRequest MakeRateLimitRequest(
		const FOpenMobileHapticPlaybackOptions& Options,
		FName Effect,
		bool bCoalescible,
		uint32 EquivalenceHash
	)
	{
		FOpenMobileHapticsRateLimitRequest Request;
		Request.Channel = Options.Channel;
		Request.Category = Options.Category;
		Request.Effect = Effect;
		Request.Priority = Options.Priority;
		Request.EquivalenceHash = EquivalenceHash;
		Request.bCoalescible = bCoalescible;
		return Request;
	}

	uint32 MakeSemanticEquivalenceHash(
		const FOpenMobileHapticSemanticRequest& Request,
		FName PatternOverride
	)
	{
		uint32 Hash = GetTypeHash(Request.Intensity);
		Hash = HashCombineFast(
			Hash,
			GetTypeHash(static_cast<uint8>(Request.Options.FallbackPolicy))
		);
	Hash = HashCombineFast(
		Hash,
		GetTypeHash(static_cast<uint8>(Request.Options.OverlapPolicy))
	);
	Hash = HashCombineFast(
		Hash,
		GetTypeHash(static_cast<uint8>(Request.Options.InterruptionPolicy))
	);
	return HashCombineFast(Hash, GetTypeHash(PatternOverride));
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

	bool ScaleMapsEqual(
		const TMap<FName, float>& Left,
		const TMap<FName, float>& Right
	)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (const TPair<FName, float>& Pair : Left)
		{
			const float* RightValue = Right.Find(Pair.Key);
			if (!RightValue || *RightValue != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}

	bool PoliciesEqual(
		const FOpenMobileHapticUserPolicy& Left,
		const FOpenMobileHapticUserPolicy& Right
	)
	{
		return Left.bEnabled == Right.bEnabled
			&& Left.bAllowCriticalFeedbackWhenDisabled
				== Right.bAllowCriticalFeedbackWhenDisabled
			&& Left.MasterIntensity == Right.MasterIntensity
			&& ScaleMapsEqual(Left.CategoryScales, Right.CategoryScales)
			&& ScaleMapsEqual(Left.EffectScales, Right.EffectScales);
	}

	FName ResolveCategory(
		FName RequestCategory,
		FName AssetOrEffectCategory,
		FName ProjectCategory
	)
	{
		if (!RequestCategory.IsNone())
		{
			return RequestCategory;
		}
		return AssetOrEffectCategory.IsNone()
			? ProjectCategory
			: AssetOrEffectCategory;
	}

	bool IsAllowedByUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy,
		EOpenMobileHapticChannelPriority Priority,
		FName Category
	)
	{
		if (Policy.bEnabled)
		{
			return true;
		}
		return Policy.bAllowCriticalFeedbackWhenDisabled
			&& Priority == EOpenMobileHapticChannelPriority::Critical
			&& (Category == TEXT("Alerts")
				|| Category == TEXT("Accessibility"));
	}

	float UserPolicyScale(
		const FOpenMobileHapticUserPolicy& Policy,
		EOpenMobileHapticChannelPriority Priority,
		FName Category,
		FName Effect
	)
	{
		if (!IsAllowedByUserPolicy(Policy, Priority, Category))
		{
			return 0.0f;
		}
		return FOpenMobileHapticsIntensityPolicy::Scale(
			1.0f,
			Policy.MasterIntensity,
			FindScale(Policy.CategoryScales, Category),
			FindScale(Policy.EffectScales, Effect),
			1.0f,
			1.0f
		);
	}

	float ActivePolicyScale(
		const FOpenMobileHapticsSubsystemRequestState& Request
	)
	{
		return Request.ResolvedUserPolicyScale;
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
		Limits.MaximumCount =
			FOpenMobileHapticsBudgetPolicy::ResolveMaximumPreparedPatterns(
				Settings.MaximumPreparedPatterns
			);
		Limits.MaximumBytes =
			FOpenMobileHapticsBudgetPolicy::ResolveMaximumPreparedPatternBytes(
				static_cast<int64>(
					Settings.MaximumPreparedPatternMemoryKilobytes
				) * 1024
			);
		Limits.IdleLifetimeSeconds =
			FOpenMobileHapticsBudgetPolicy::
				ResolvePreparedIdleLifetimeSeconds(
					Settings.PreparedPatternIdleLifetimeSeconds
				);
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
		const int32 MaximumEvents =
			FOpenMobileHapticsBudgetPolicy::ResolveMaximumDiagnosticEvents(
				GetDefault<UOpenMobileHapticsSettings>()->MaximumDiagnosticEvents
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
			if (State.NativeEventDispatcher)
			{
				State.NativeEventDispatcher->UnregisterToken(Request->Token);
			}
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
			if (Request->OverlapQueueExpiryTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(
					Request->OverlapQueueExpiryTickerHandle
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
		State.ChannelArbiter.Release(RequestId);
		State.Requests.Remove(RequestId);
	}

	void PreserveOverlapQueueLifecycle(
		FOpenMobileHapticsSubsystemState& State,
		uint64 RequestId,
		FOpenMobileHapticsSubsystemRequestState& RequestState
	)
	{
		const FOpenMobileHapticsSubsystemRequestState* Existing =
			State.Requests.Find(RequestId);
		if (!Existing || !Existing->bWaitingForOverlap)
		{
			return;
		}
		if (Existing->OverlapQueueExpiryTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(
				Existing->OverlapQueueExpiryTickerHandle
			);
		}
		RequestState.LastPublishedState = Existing->LastPublishedState;
		RequestState.LastEventTimestampSeconds =
			Existing->LastEventTimestampSeconds;
		RequestState.SubmissionState = Existing->SubmissionState;
		RequestState.SubmissionTimestampSeconds =
			Existing->SubmissionTimestampSeconds;
		RequestState.RecoverySourceHandle = Existing->RecoverySourceHandle;
		RequestState.bPromotedFromOverlapQueue = true;
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
		FOpenMobileHapticsSubsystemRequestState* ExistingRequest =
			State.Requests.Find(Token.RequestId);
		const bool bPromotedFromOverlapQueue = ExistingRequest
			&& ExistingRequest->bPromotedFromOverlapQueue;
		State.LastResolvedPath = Result.ResolvedPath;
		State.LastFallbackAttempts = Result.FallbackAttempts;
		if (Result.Outcome == EOpenMobileHapticPlaybackOutcome::Suppressed)
		{
			Result.Handle = bPromotedFromOverlapQueue
				? Token.PlaybackHandle
				: FOpenMobileHapticPlaybackHandle{};
			if (Result.State == EOpenMobileHapticPlaybackState::Invalid)
			{
				Result.State = EOpenMobileHapticPlaybackState::Completed;
			}
			if (!bPromotedFromOverlapQueue)
			{
				RemoveRequest(State, Token.RequestId);
			}
			return Result;
		}
		if (!Result.IsAccepted())
		{
			Result.Handle = bPromotedFromOverlapQueue
				? Token.PlaybackHandle
				: FOpenMobileHapticPlaybackHandle{};
			if (!bPromotedFromOverlapQueue)
			{
				RemoveRequest(State, Token.RequestId);
			}
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
			if (!bPromotedFromOverlapQueue)
			{
				RemoveRequest(State, Token.RequestId);
			}
			FOpenMobileHapticsErrorContext Context;
			Context.Reason = EOpenMobileHapticsFailureReason::Internal;
			Context.Stage = EOpenMobileHapticFailureStage::NativeSubmission;
			Context.Channel = Channel;
			Context.Handle = Token.PlaybackHandle;
			Result = FOpenMobileHapticPlaybackResult::MakeRejected(
				FOpenMobileHapticsErrorMapper::Map(Context)
			);
			Result.Handle = bPromotedFromOverlapQueue
				? Token.PlaybackHandle
				: FOpenMobileHapticPlaybackHandle{};
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
			if (bPromotedFromOverlapQueue)
			{
				Result.Handle = Token.PlaybackHandle;
				State.RequestByHandle.Add(
					Token.PlaybackHandle,
					Token.RequestId
				);
				State.PlaybackStates.Add(Token.PlaybackHandle, Result.State);
				if (FOpenMobileHapticsSubsystemRequestState* Request =
					State.Requests.Find(Token.RequestId))
				{
					Request->bFireAndForgetAfterOverlapPromotion = true;
				}
			}
			else
			{
				Result.Handle = {};
			}
		}
		if (!Submission.bExpectsCallbacks && !bPromotedFromOverlapQueue)
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
	bLegacyPreparationClaim = false;
	ActivePreparationActions.Reset();
	ActiveNamedPlaybackActions.Reset();
	ActivePreparationLeases.Reset();
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	UserPolicy = {};
	UserPolicy.bEnabled = Settings->bEnabledByDefault;
	UserPolicy.bAllowCriticalFeedbackWhenDisabled =
		Settings->bAllowCriticalFeedbackWhenDisabledByDefault;
	UserPolicy.MasterIntensity =
		FMath::IsFinite(Settings->DefaultMasterIntensity)
		&& Settings->DefaultMasterIntensity >= 0.0f
		&& Settings->DefaultMasterIntensity <= 1.0f
			? Settings->DefaultMasterIntensity
			: 1.0f;
	bUserPolicyEnabled.Store(UserPolicy.bEnabled);
	State.Reset(new FOpenMobileHapticsSubsystemState());
	EnsureNativeEventDispatcher(*State);
	State->RecentPlaybackEvents.Reserve(
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumDiagnosticEvents(
			Settings->MaximumDiagnosticEvents
		)
	);
	State->DynamicParameterBatch.Reserve(
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumActiveHandles(
			Settings->MaximumActiveHandles
		)
		+ FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueuedHandles(
			Settings->MaximumQueuedHandles
		)
	);
	BindRecoveryEvents();
	LastBroadcastCapabilities = GetCapabilitiesNative();
}

void UOpenMobileHapticsSubsystem::Deinitialize()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_Shutdown);
	if (bDeinitialized)
	{
		return;
	}
	UnbindRecoveryEvents();
	if (State && State->NativeEventDispatcher)
	{
		State->NativeEventDispatcher->Close();
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
				if (!Pair.Value.bWaitingForOverlap
					&& Pair.Value.Token.PlaybackHandle.IsValid()
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
	TArray<TWeakObjectPtr<UOpenMobileHapticPreparationAsyncAction>>
		PreparationActions;
	PreparationActions.Reserve(ActivePreparationActions.Num());
	for (const TWeakObjectPtr<UOpenMobileHapticPreparationAsyncAction>& Action :
		ActivePreparationActions)
	{
		PreparationActions.Add(Action);
	}
	ActivePreparationActions.Reset();
	for (const TWeakObjectPtr<UOpenMobileHapticPreparationAsyncAction>& Action :
		PreparationActions)
	{
		if (Action.IsValid())
		{
			Action->HandleGameInstanceTeardown();
		}
	}
	TArray<TWeakObjectPtr<UOpenMobileHapticNamedPlaybackAsyncAction>>
		NamedPlaybackActions;
	NamedPlaybackActions.Reserve(ActiveNamedPlaybackActions.Num());
	for (const TWeakObjectPtr<UOpenMobileHapticNamedPlaybackAsyncAction>& Action :
		ActiveNamedPlaybackActions)
	{
		NamedPlaybackActions.Add(Action);
	}
	ActiveNamedPlaybackActions.Reset();
	for (const TWeakObjectPtr<UOpenMobileHapticNamedPlaybackAsyncAction>& Action :
		NamedPlaybackActions)
	{
		if (Action.IsValid())
		{
			Action->HandleGameInstanceTeardown();
		}
	}
	TArray<TWeakObjectPtr<UOpenMobileHapticPreparationLease>> Leases;
	Leases.Reserve(ActivePreparationLeases.Num());
	for (const TWeakObjectPtr<UOpenMobileHapticPreparationLease>& Lease :
		ActivePreparationLeases)
	{
		Leases.Add(Lease);
	}
	ActivePreparationLeases.Reset();
	for (const TWeakObjectPtr<UOpenMobileHapticPreparationLease>& Lease : Leases)
	{
		if (Lease.IsValid())
		{
			Lease->HandleGameInstanceTeardown();
		}
	}
	bLegacyPreparationClaim = false;
	TArray<TObjectPtr<UOpenMobileHapticPlayback>> Playbacks;
	Playbacks.Reserve(ActivePlaybackObjects.Num());
	for (UOpenMobileHapticPlayback* Playback : ActivePlaybackObjects)
	{
		Playbacks.Add(Playback);
	}
	ActivePlaybackObjects.Reset();
	for (UOpenMobileHapticPlayback* Playback : Playbacks)
	{
		if (IsValid(Playback))
		{
			Playback->HandleGameInstanceTeardown();
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
	OnPreparationStateChanged.Clear();
	OnPolicyChanged.Clear();
	OnAvailabilityChanged.Clear();
	OnCapabilitiesChanged.Clear();
	OnMasterIntensityChanged.Clear();
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
	return TrackInitialSubmissionResult(
		SubmitSemanticOrOverride(
			Request,
			OpenMobileHapticsSubsystemPrivate::FindLoadedGamePresetOverride(
				*Settings,
				Preset
			)
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

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitPatternAsset(
	UOpenMobileHapticPatternAsset* PatternAsset,
	float Intensity,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	check(IsInGameThread());
	if (!PatternAsset || !PatternAsset->IsDerivedDataCurrent())
	{
		FOpenMobileHapticError Error = FOpenMobileHapticError::FromCommon(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The selected Haptic Pattern has invalid cooked data."),
			EOpenMobileHapticFailureStage::Validation
		);
		Error.Correction =
			TEXT("Open and save the pattern asset, then resolve its validation errors.");
		return FOpenMobileHapticPlaybackResult::MakeRejected(MoveTemp(Error));
	}
	const FSoftObjectPath Override =
		PatternAsset->GetOverrideForCurrentPlatform();
	if (!Override.IsNull() && !Override.ResolveObject())
	{
		FOpenMobileHapticError Error = FOpenMobileHapticError::FromCommon(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("The selected pattern's platform override is not prepared."),
			EOpenMobileHapticFailureStage::Preparation
		);
		Error.Correction =
			TEXT("Use Play Haptic Pattern Asset with Prepare If Needed enabled.");
		return FOpenMobileHapticPlaybackResult::MakeRejected(MoveTemp(Error));
	}

	FOpenMobileHapticNamedPatternRequest Request;
	Request.PatternName = PatternAsset->GetFName();
	Request.PatternAsset = FSoftObjectPath(PatternAsset);
	Request.PlatformOverrideAsset = Override;
	Request.Intensity = Intensity;
	Request.Options = Options;
	return TrackInitialSubmissionResult(
		SubmitNamedPatternInternal(Request, nullptr, true)
	);
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

bool UOpenMobileHapticsSubsystem::GetTimingCalibrationPrecision(
	EOpenMobileHapticTimingClock Clock,
	double& OutEstimatedPrecisionSeconds
) const
{
	check(IsInGameThread());
	return State && State->TimingPolicy.GetCalibrationPrecision(
		Clock,
		static_cast<int64>(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
		),
		OutEstimatedPrecisionSeconds
	);
}

FOpenMobileHapticLibraryPreloadHandle
UOpenMobileHapticsSubsystem::PreloadNamedLibraries()
{
	return PreloadNamedLibrariesInternal(true);
}

FOpenMobileHapticLibraryPreloadHandle
UOpenMobileHapticsSubsystem::PreloadNamedLibrariesInternal(
	bool bAddLegacyClaim
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_PreloadNamedLibraries);
	check(IsInGameThread());
	FOpenMobileHapticLibraryPreloadHandle Handle;
	const bool bPreviousLegacyClaim = bLegacyPreparationClaim;
	if (bAddLegacyClaim)
	{
		bLegacyPreparationClaim = true;
	}
	if (bDeinitialized)
	{
		bLegacyPreparationClaim = bPreviousLegacyClaim;
		return Handle;
	}
	if (!FOpenMobileHapticsBackendRegistry::RequestRecovery(
		UserPolicy.bEnabled
	))
	{
		bLegacyPreparationClaim = bPreviousLegacyClaim;
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
		State->ActivePreparationStartTimeSeconds = FPlatformTime::Seconds();
		TArray<FString> Errors;
		const bool bRequiresNativePreparation =
			FOpenMobileHapticsBackendRegistry::FindBackend()
			&& GetPreparationState()
				!= EOpenMobileHapticPreparationState::Prepared;
		if (bRequiresNativePreparation)
		{
			const EOpenMobileHapticPreparationState PreviousState =
				State->PreparationState;
			State->PreparationState =
				EOpenMobileHapticPreparationState::Preparing;
			BroadcastPreparationStateChange(
				PreviousState,
				State->PreparationState,
				TEXT("The active backend is preparing loaded Haptics resources."),
				true
			);
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
	LocalState.ActivePreparationStartTimeSeconds = FPlatformTime::Seconds();
	const uint64 Generation = LocalState.LibraryResolver.BeginPreparation();
	LocalState.LastNamedPatternStatus =
		EOpenMobileHapticNamedPatternStatus::Loading;
	const EOpenMobileHapticPreparationState PreviousState =
		LocalState.PreparationState;
	LocalState.PreparationState =
		EOpenMobileHapticPreparationState::Preparing;
	BroadcastPreparationStateChange(
		PreviousState,
		LocalState.PreparationState,
		TEXT("Configured Haptics libraries are loading."),
		false
	);

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
	bLegacyPreparationClaim = false;
	ReleaseNamedLibrariesInternal(true);
	FOpenMobileHapticControlResult Result;
	Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
	return Result;
}

void UOpenMobileHapticsSubsystem::ReleaseNamedLibraries()
{
	check(IsInGameThread());
	bLegacyPreparationClaim = false;
	for (auto Iterator = ActivePreparationLeases.CreateIterator(); Iterator;
		++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}
	if (ActivePreparationLeases.IsEmpty())
	{
		ReleaseNamedLibrariesInternal(true);
	}
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

void UOpenMobileHapticsSubsystem::GetPreparedPatternNames(
	TArray<FName>& OutPatternNames
) const
{
	check(IsInGameThread());
	OutPatternNames.Reset();
	if (!State)
	{
		return;
	}
	TArray<TPair<FName, FSoftObjectPath>> PreparedPatterns;
	State->LibraryResolver.GetPreparedPatterns(PreparedPatterns);
	OutPatternNames.Reserve(PreparedPatterns.Num());
	for (const TPair<FName, FSoftObjectPath>& Pattern : PreparedPatterns)
	{
		OutPatternNames.Add(Pattern.Key);
	}
	OutPatternNames.Sort(FNameLexicalLess());
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
	const double PreparationStartTimeSeconds = FPlatformTime::Seconds();
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
	LocalState.PerformanceTracker.RecordPreparationLatencySeconds(
		FPlatformTime::Seconds() - PreparationStartTimeSeconds
	);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_PrepareLoadedLibraries);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_PrepareLoadedPatterns);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_PrepareLoadedOverrides);
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
	if (State->ActivePreparationStartTimeSeconds >= 0.0)
	{
		State->PerformanceTracker.RecordPreparationLatencySeconds(
			FPlatformTime::Seconds()
				- State->ActivePreparationStartTimeSeconds
		);
		State->ActivePreparationStartTimeSeconds = -1.0;
	}
	FOpenMobileHapticLibraryPreloadResult Result;
	Result.Handle = Handle;
	Result.Outcome = Outcome;
	Result.PreparedPatternCount =
		State->LibraryResolver.GetPreparedPatternCount();
	Result.Errors = MoveTemp(Errors);
	const EOpenMobileHapticPreparationState PreviousState =
		State->ActiveLibraryPreload.IsValid()
			? EOpenMobileHapticPreparationState::Preparing
			: State->PreparationState;
	State->PreparationState = Outcome
		== EOpenMobileHapticLibraryPreloadOutcome::Prepared
			? EOpenMobileHapticPreparationState::Prepared
			: Outcome == EOpenMobileHapticLibraryPreloadOutcome::Cancelled
				? EOpenMobileHapticPreparationState::Unprepared
				: EOpenMobileHapticPreparationState::Failed;
	BroadcastPreparationStateChange(
		PreviousState,
		State->PreparationState,
		Outcome == EOpenMobileHapticLibraryPreloadOutcome::Prepared
			? TEXT("Configured Haptics content is ready.")
			: Outcome == EOpenMobileHapticLibraryPreloadOutcome::Cancelled
				? TEXT("Haptics preparation was cancelled.")
				: TEXT("Haptics preparation failed."),
		Outcome == EOpenMobileHapticLibraryPreloadOutcome::Prepared
	);
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
	for (auto Iterator = ActivePreparationLeases.CreateIterator(); Iterator;
		++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}
	if (Outcome == EOpenMobileHapticLibraryPreloadOutcome::Prepared
		&& ActivePreparationLeases.IsEmpty()
		&& !bLegacyPreparationClaim)
	{
		ReleaseNamedLibrariesInternal(false);
	}
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
	if (ActiveHandle.IsValid()
		&& State->ActivePreparationStartTimeSeconds >= 0.0)
	{
		State->PerformanceTracker.RecordPreparationLatencySeconds(
			FPlatformTime::Seconds()
				- State->ActivePreparationStartTimeSeconds
		);
	}
	State->ActivePreparationStartTimeSeconds = -1.0;
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
	BroadcastPreparationStateChange(
		PreviousPreparationState,
		State->PreparationState,
		TEXT("The final Haptics preparation owner released its claim."),
		false
	);
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

bool UOpenMobileHapticsSubsystem::IsHapticsEnabled() const
{
	return IsHapticsEnabledNative();
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::SetHapticsEnabled(
	bool bEnabled
)
{
	return SetHapticsEnabledNative(bEnabled);
}

float UOpenMobileHapticsSubsystem::GetMasterIntensity() const
{
	return GetMasterIntensityNative();
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::SetMasterIntensity(
	float MasterIntensity
)
{
	return SetMasterIntensityNative(MasterIntensity);
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

bool UOpenMobileHapticsSubsystem::AdmitChannelRequest(
	const FOpenMobileHapticsBackendRequestToken& Token,
	const FOpenMobileHapticPlaybackOptions& Options,
	int32 MaximumActiveHandles,
	int32 MaximumQueueDepth,
	bool bQueued,
	bool bWaitingForOverlap,
	bool bRepeating,
	FName Effect,
	FOpenMobileHapticPlaybackResult& OutRejection
)
{
	if (!Token.PlaybackHandle.IsValid())
	{
		return true;
	}

	FOpenMobileHapticsSubsystemState* LocalState = &GetOrCreateState();
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	FOpenMobileHapticsChannelLimits Limits;
	Limits.MaximumActiveHandles = Settings->MaximumActiveHandles;
	Limits.MaximumQueuedHandles = Settings->MaximumQueuedHandles;
	Limits.MaximumQueueDepthPerChannel =
		Settings->MaximumQueueDepthPerChannel;
	LocalState->ChannelArbiter.Configure(Limits);

	FOpenMobileHapticsChannelAdmissionRequest AdmissionRequest;
	AdmissionRequest.RequestId = Token.RequestId;
	AdmissionRequest.Channel = Options.Channel;
	AdmissionRequest.Priority = Options.Priority;
	AdmissionRequest.MaximumActiveHandles = MaximumActiveHandles;
	AdmissionRequest.MaximumQueueDepth = MaximumQueueDepth;
	AdmissionRequest.bQueued = bQueued;
	AdmissionRequest.bWaitingForOverlap = bWaitingForOverlap;
	AdmissionRequest.bRepeating = bRepeating;

	const int32 MaximumAttempts =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumActiveHandles(
			Settings->MaximumActiveHandles
		) + 1;
	for (int32 Attempt = 0; Attempt < MaximumAttempts; ++Attempt)
	{
		const FOpenMobileHapticsChannelAdmissionResult Admission =
			LocalState->ChannelArbiter.TryReserve(AdmissionRequest);
		if (Admission.Outcome
			== EOpenMobileHapticsChannelAdmissionOutcome::Admitted)
		{
			LocalState->PerformanceTracker.RecordQueueDepth(
				LocalState->ChannelArbiter.GetQueuedCount()
			);
			return true;
		}
		if (Admission.Outcome
			!= EOpenMobileHapticsChannelAdmissionOutcome::PreemptRequired)
		{
			break;
		}

		FOpenMobileHapticsSubsystemRequestState* Existing =
			LocalState->Requests.Find(Admission.PreemptRequestId);
		if (!Existing)
		{
			LocalState->ChannelArbiter.Release(Admission.PreemptRequestId);
			continue;
		}
		if (!Existing->Token.PlaybackHandle.IsValid())
		{
			break;
		}

		EndPlaybackNative(
			Existing->Token.PlaybackHandle,
			EOpenMobileHapticPlaybackState::Interrupted
		);
		IOpenMobileHapticsBackend* CurrentBackend = bDeinitialized
			? nullptr
			: FOpenMobileHapticsBackendRegistry::FindBackend();
		if (bDeinitialized
			|| !State
			|| !CurrentBackend
			|| CurrentBackend->GetBackendName() != Token.BackendName)
		{
			OutRejection =
				OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
					EOpenMobileHapticsFailureReason::BackendUnavailable,
					EOpenMobileHapticFailureStage::Channel,
					Effect,
					Options.Channel
				);
			return false;
		}
		LocalState = State.Get();
		if (!LocalState->Requests.Contains(Admission.PreemptRequestId))
		{
			continue;
		}
		break;
	}

	OutRejection =
		OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::BusyChannel,
			EOpenMobileHapticFailureStage::Channel,
			Effect,
			Options.Channel
		);
	if (State)
	{
		State->LastError = OutRejection.Error;
	}
	return false;
}

bool UOpenMobileHapticsSubsystem::ResolveAndApplyOverlap(
	const FOpenMobileHapticPlaybackOptions& Options,
	FName Effect,
	uint64 ExcludedRequestId,
	FOpenMobileHapticPlaybackResult& OutResult,
	bool& bOutShouldQueue,
	bool& bOutUsedMixFallback
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_ResolveOverlap);
	bOutShouldQueue = false;
	bOutUsedMixFallback = false;
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const int32 MaximumAttempts =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumActiveHandles(
			Settings->MaximumActiveHandles
		)
		+ FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueuedHandles(
			Settings->MaximumQueuedHandles
		)
		+ 1;
	for (int32 Attempt = 0; Attempt < MaximumAttempts; ++Attempt)
	{
		if (bDeinitialized || !State)
		{
			OutResult =
				OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
					EOpenMobileHapticsFailureReason::BackendUnavailable,
					EOpenMobileHapticFailureStage::Channel,
					Effect,
					Options.Channel
				);
			return false;
		}

		TArray<FOpenMobileHapticsOverlapConflict> Conflicts;
		Conflicts.Reserve(State->Requests.Num());
		for (const TPair<uint64, FOpenMobileHapticsSubsystemRequestState>& Pair :
			State->Requests)
		{
			if (Pair.Key == ExcludedRequestId
				|| (ExcludedRequestId != 0
					&& Pair.Value.bWaitingForOverlap)
				|| !Pair.Value.Token.PlaybackHandle.IsValid())
			{
				continue;
			}
			FOpenMobileHapticsOverlapConflict Conflict;
			Conflict.RequestId = Pair.Key;
			Conflict.Channel = Pair.Value.Channel;
			Conflict.Priority = Pair.Value.Priority;
			Conflict.bQueued = Pair.Value.bWaitingForOverlap;
			Conflicts.Add(Conflict);
		}

		const FOpenMobileHapticsResolvedChannel ResolvedChannel =
			FOpenMobileHapticsChannelPolicy::Resolve(
				Options.Channel,
				Options.Priority,
				Settings->Channels,
				Settings->MaximumActiveHandles,
				Settings->MaximumQueueDepthPerChannel
			);
		FOpenMobileHapticsOverlapRequest OverlapRequest;
		OverlapRequest.Channel = Options.Channel;
		OverlapRequest.Priority = ResolvedChannel.EffectivePriority;
		OverlapRequest.Policy = Options.OverlapPolicy;
		OverlapRequest.Mixing =
			FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot().Mixing;
		OverlapRequest.UnsupportedMixFallback =
			ResolvedChannel.UnsupportedMixFallbackPolicy;
		FOpenMobileHapticsOverlapResolution Resolution =
			FOpenMobileHapticsOverlapPolicy::Resolve(
				OverlapRequest,
				Conflicts
			);
		bOutUsedMixFallback = bOutUsedMixFallback
			|| Resolution.bUsedMixFallback;
		if (Resolution.Outcome == EOpenMobileHapticsOverlapOutcome::Submit)
		{
			return true;
		}
		if (Resolution.Outcome == EOpenMobileHapticsOverlapOutcome::Queue)
		{
			bOutShouldQueue = true;
			return true;
		}
		if (Resolution.Outcome == EOpenMobileHapticsOverlapOutcome::Suppress)
		{
			const FName Reason = Resolution.ResolvedPolicy
				== EOpenMobileHapticOverlapPolicy::Ignore
					? FName(TEXT("OverlapIgnored"))
					: FName(TEXT("OverlapPriority"));
			OutResult =
				OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
					Options.Channel,
					Reason
				);
			return false;
		}

		for (const uint64 RequestId : Resolution.TerminalRequestIds)
		{
			if (bDeinitialized || !State)
			{
				break;
			}
			FOpenMobileHapticsSubsystemRequestState* Existing =
				State->Requests.Find(RequestId);
			if (!Existing)
			{
				continue;
			}
			FOpenMobileHapticControlResult EndResult;
			if (Existing->bWaitingForOverlap)
			{
				CompleteControlledRequest(
					RequestId,
					EOpenMobileHapticPlaybackState::Cancelled
				);
				EndResult.Outcome = EOpenMobileHapticControlOutcome::Accepted;
			}
			else
			{
				const FOpenMobileHapticPlaybackHandle ExistingHandle =
					Existing->Token.PlaybackHandle;
				EndResult = EndPlaybackNative(
					ExistingHandle,
					EOpenMobileHapticPlaybackState::Interrupted
				);
			}
			if (EndResult.Outcome != EOpenMobileHapticControlOutcome::Accepted
				&& State && State->Requests.Contains(RequestId))
			{
				OutResult =
					OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
						EOpenMobileHapticsFailureReason::BusyChannel,
						EOpenMobileHapticFailureStage::Channel,
						Effect,
						Options.Channel
					);
				State->LastError = OutResult.Error;
				return false;
			}
		}
	}

	OutResult = OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
		EOpenMobileHapticsFailureReason::BusyChannel,
		EOpenMobileHapticFailureStage::Channel,
		Effect,
		Options.Channel
	);
	if (State)
	{
		State->LastError = OutResult.Error;
	}
	return false;
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::QueueOverlapRequest(
	const FOpenMobileHapticPlaybackOptions& Options,
	const FOpenMobileHapticsResolvedChannel& ResolvedChannel,
	FName Effect,
	bool bRepeating,
	bool bUsedMixFallback,
	const FOpenMobileHapticsBackendRequestToken* ExistingToken,
	const FOpenMobileHapticSemanticRequest* SemanticRequest,
	const FOpenMobileHapticOneShotRequest* OneShotRequest,
	const FOpenMobileHapticNamedPatternRequest* NamedRequest,
	FName SemanticPatternOverride
)
{
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	if (ExistingToken)
	{
		const FOpenMobileHapticsSubsystemRequestState* Existing =
			LocalState.Requests.Find(ExistingToken->RequestId);
		if (!Existing || !Existing->bWaitingForOverlap)
		{
			return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
				EOpenMobileHapticsFailureReason::BackendUnavailable,
				EOpenMobileHapticFailureStage::Channel,
				Effect,
				Options.Channel
			);
		}
		FOpenMobileHapticPlaybackResult Result;
		Result.Outcome = bUsedMixFallback
			? EOpenMobileHapticPlaybackOutcome::Fallback
			: EOpenMobileHapticPlaybackOutcome::Accepted;
		Result.State = EOpenMobileHapticPlaybackState::Scheduled;
		Result.Handle = ExistingToken->PlaybackHandle;
		Result.Channel = Options.Channel;
		Result.ResolvedPath = Existing->ResolvedPath;
		return Result;
	}

	IOpenMobileHapticsBackend* Backend = bDeinitialized
		? nullptr
		: FOpenMobileHapticsBackendRegistry::FindBackend();
	if (!Backend)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeUnsupportedPlaybackResult();
	}
	const FOpenMobileHapticsBackendRequestToken Token =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(*Backend, true);
	if (!Token.IsValid() || !Token.PlaybackHandle.IsValid())
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
			EOpenMobileHapticsFailureReason::BackendUnavailable,
			EOpenMobileHapticFailureStage::Channel,
			Effect,
			Options.Channel
		);
	}
	FOpenMobileHapticPlaybackResult AdmissionRejection;
	if (!AdmitChannelRequest(
		Token,
		Options,
		ResolvedChannel.MaximumActiveHandles,
		ResolvedChannel.MaximumQueueDepth,
		true,
		true,
		bRepeating,
		Effect,
		AdmissionRejection
	))
	{
		return AdmissionRejection;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	FOpenMobileHapticsSubsystemRequestState RequestState;
	RequestState.Token = Token;
	RequestState.Channel = Options.Channel;
	RequestState.Category = Options.Category;
	RequestState.Effect = Effect;
	RequestState.Priority = ResolvedChannel.EffectivePriority;
	RequestState.ResolvedUserPolicyScale =
		OpenMobileHapticsSubsystemPrivate::UserPolicyScale(
			UserPolicy,
			RequestState.Priority,
			RequestState.Category,
			RequestState.Effect
		);
	RequestState.ResolvedPath = bUsedMixFallback
		? FName(TEXT("MixFallbackQueue"))
		: FName(TEXT("OverlapQueue"));
	RequestState.SubmissionState = EOpenMobileHapticPlaybackState::Scheduled;
	RequestState.SubmissionTimestampSeconds = NowSeconds;
	RequestState.OverlapEnqueuedAtSeconds = NowSeconds;
	RequestState.bWaitingForOverlap = true;
	if (SemanticRequest)
	{
		RequestState.QueuedSemanticRequest = *SemanticRequest;
		RequestState.QueuedSemanticPatternOverride = SemanticPatternOverride;
	}
	if (OneShotRequest)
	{
		RequestState.QueuedOneShotRequest = *OneShotRequest;
	}
	if (NamedRequest)
	{
		RequestState.QueuedNamedRequest = *NamedRequest;
	}
	LocalState.Requests.Add(Token.RequestId, MoveTemp(RequestState));
	LocalState.NativeEventDispatcher->RegisterToken(Token);
	LocalState.RequestByHandle.Add(Token.PlaybackHandle, Token.RequestId);
	LocalState.PlaybackStates.Add(
		Token.PlaybackHandle,
		EOpenMobileHapticPlaybackState::Scheduled
	);

	const TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakSubsystem(this);
	AsyncTask(
		ENamedThreads::GameThread,
		[WeakSubsystem, RequestId = Token.RequestId]()
		{
			if (UOpenMobileHapticsSubsystem* Subsystem = WeakSubsystem.Get())
			{
				Subsystem->PublishDeferredSubmissionEvents(RequestId);
			}
		}
	);
	ScheduleOverlapQueueExpiry(
		Token.RequestId,
		GetDefault<UOpenMobileHapticsSettings>()
			->MaximumQueuedRequestAgeSeconds
	);

	FOpenMobileHapticPlaybackResult Result;
	Result.Outcome = bUsedMixFallback
		? EOpenMobileHapticPlaybackOutcome::Fallback
		: EOpenMobileHapticPlaybackOutcome::Accepted;
	Result.State = EOpenMobileHapticPlaybackState::Scheduled;
	Result.Handle = Token.PlaybackHandle;
	Result.Channel = Options.Channel;
	Result.ResolvedPath = bUsedMixFallback
		? FName(TEXT("MixFallbackQueue"))
		: FName(TEXT("OverlapQueue"));
	return Result;
}

void UOpenMobileHapticsSubsystem::ScheduleOverlapQueueExpiry(
	uint64 RequestId,
	double DelaySeconds
)
{
	if (bDeinitialized || RequestId == 0)
	{
		return;
	}
	FOpenMobileHapticsSubsystemRequestState* Request =
		GetOrCreateState().Requests.Find(RequestId);
	if (!Request || !Request->bWaitingForOverlap
		|| Request->OverlapQueueExpiryTickerHandle.IsValid())
	{
		return;
	}
	const TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakSubsystem(this);
	Request->OverlapQueueExpiryTickerHandle =
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda(
				[WeakSubsystem, RequestId](float)
				{
					if (UOpenMobileHapticsSubsystem* Subsystem =
						WeakSubsystem.Get())
					{
						Subsystem->ExpireOverlapQueue(RequestId);
					}
					return false;
				}
			),
			static_cast<float>(FMath::Clamp(DelaySeconds, 0.001, 30.001))
		);
}

void UOpenMobileHapticsSubsystem::ExpireOverlapQueue(uint64 RequestId)
{
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	FOpenMobileHapticsSubsystemRequestState* Request =
		State->Requests.Find(RequestId);
	if (!Request || !Request->bWaitingForOverlap)
	{
		return;
	}
	Request->OverlapQueueExpiryTickerHandle.Reset();
	const double NowSeconds = FPlatformTime::Seconds();
	const double MaximumAgeSeconds = FMath::Clamp<double>(
		GetDefault<UOpenMobileHapticsSettings>()
			->MaximumQueuedRequestAgeSeconds,
		0.01,
		30.0
	);
	if (!FOpenMobileHapticsOverlapPolicy::IsExpired(
		Request->OverlapEnqueuedAtSeconds,
		NowSeconds,
		MaximumAgeSeconds
	))
	{
		const double RemainingSeconds = FMath::Max(
			0.001,
			Request->OverlapEnqueuedAtSeconds + MaximumAgeSeconds
				- NowSeconds + 0.001
		);
		ScheduleOverlapQueueExpiry(RequestId, RemainingSeconds);
		return;
	}
	Request->ResolvedPath = TEXT("OverlapQueueExpired");
	CompleteControlledRequest(
		RequestId,
		EOpenMobileHapticPlaybackState::Cancelled
	);
}

void UOpenMobileHapticsSubsystem::ScheduleOverlapQueueDrain()
{
	if (bDeinitialized || !State || State->bOverlapQueueDrainScheduled)
	{
		return;
	}
	State->bOverlapQueueDrainScheduled = true;
	const TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakSubsystem(this);
	AsyncTask(
		ENamedThreads::GameThread,
		[WeakSubsystem]()
		{
			UOpenMobileHapticsSubsystem* Subsystem = WeakSubsystem.Get();
			if (!Subsystem || Subsystem->bDeinitialized || !Subsystem->State)
			{
				return;
			}
			Subsystem->State->bOverlapQueueDrainScheduled = false;
			Subsystem->DrainOverlapQueues(FPlatformTime::Seconds());
		}
	);
}

void UOpenMobileHapticsSubsystem::DrainOverlapQueues(double NowSeconds)
{
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	const int32 MaximumAttempts = FMath::Clamp(
		GetDefault<UOpenMobileHapticsSettings>()->MaximumQueuedHandles,
		1,
		256
	);
	for (int32 Attempt = 0; Attempt < MaximumAttempts; ++Attempt)
	{
		TArray<FOpenMobileHapticsOverlapQueueEntry> Queue;
		TSet<FName> ActiveChannels;
		for (const TPair<uint64, FOpenMobileHapticsSubsystemRequestState>& Pair :
			State->Requests)
		{
			if (Pair.Value.bWaitingForOverlap)
			{
				FOpenMobileHapticsOverlapQueueEntry Entry;
				Entry.RequestId = Pair.Key;
				Entry.Channel = Pair.Value.Channel;
				Entry.Priority = Pair.Value.Priority;
				Entry.EnqueuedAtSeconds =
					Pair.Value.OverlapEnqueuedAtSeconds;
				Queue.Add(Entry);
			}
			else
			{
				ActiveChannels.Add(Pair.Value.Channel);
			}
		}
		if (Queue.IsEmpty())
		{
			return;
		}
		const double MaximumAgeSeconds = FMath::Clamp<double>(
			GetDefault<UOpenMobileHapticsSettings>()
				->MaximumQueuedRequestAgeSeconds,
			0.01,
			30.0
		);
		bool bExpiredRequest = false;
		for (const FOpenMobileHapticsOverlapQueueEntry& Entry : Queue)
		{
			if (!FOpenMobileHapticsOverlapPolicy::IsExpired(
				Entry.EnqueuedAtSeconds,
				NowSeconds,
				MaximumAgeSeconds
			))
			{
				continue;
			}
			if (FOpenMobileHapticsSubsystemRequestState* Current =
				State->Requests.Find(Entry.RequestId))
			{
				Current->ResolvedPath = TEXT("OverlapQueueExpired");
			}
			CompleteControlledRequest(
				Entry.RequestId,
				EOpenMobileHapticPlaybackState::Cancelled
			);
			bExpiredRequest = true;
		}
		if (bExpiredRequest)
		{
			continue;
		}

		TSet<FName> QueuedChannels;
		for (const FOpenMobileHapticsOverlapQueueEntry& Entry : Queue)
		{
			QueuedChannels.Add(Entry.Channel);
		}
		uint64 SelectedRequestId = 0;
		EOpenMobileHapticChannelPriority SelectedPriority =
			EOpenMobileHapticChannelPriority::Low;
		for (const FName Channel : QueuedChannels)
		{
			if (ActiveChannels.Contains(Channel))
			{
				continue;
			}
			const uint64 CandidateId =
				FOpenMobileHapticsOverlapPolicy::SelectNext(Channel, Queue);
			const FOpenMobileHapticsSubsystemRequestState* Candidate =
				State->Requests.Find(CandidateId);
			if (!Candidate)
			{
				continue;
			}
			if (SelectedRequestId == 0
				|| static_cast<uint8>(Candidate->Priority)
					> static_cast<uint8>(SelectedPriority)
				|| (Candidate->Priority == SelectedPriority
					&& CandidateId < SelectedRequestId))
			{
				SelectedRequestId = CandidateId;
				SelectedPriority = Candidate->Priority;
			}
		}
		if (SelectedRequestId == 0)
		{
			return;
		}

		FOpenMobileHapticsSubsystemRequestState QueuedRequest =
			State->Requests.FindChecked(SelectedRequestId);
		if (FOpenMobileHapticsOverlapPolicy::IsExpired(
			QueuedRequest.OverlapEnqueuedAtSeconds,
			NowSeconds,
			MaximumAgeSeconds
		))
		{
			if (FOpenMobileHapticsSubsystemRequestState* Current =
				State->Requests.Find(SelectedRequestId))
			{
				Current->ResolvedPath = TEXT("OverlapQueueExpired");
			}
			CompleteControlledRequest(
				SelectedRequestId,
				EOpenMobileHapticPlaybackState::Cancelled
			);
			continue;
		}
		IOpenMobileHapticsBackend* Backend =
			FOpenMobileHapticsBackendRegistry::FindBackend();
		if (!Backend
			|| Backend->GetBackendName() != QueuedRequest.Token.BackendName
			|| !FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(
				QueuedRequest.Token
			))
		{
			const FOpenMobileHapticPlaybackResult Result =
				OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
					EOpenMobileHapticsFailureReason::BackendUnavailable,
					EOpenMobileHapticFailureStage::Channel,
					QueuedRequest.Effect,
					QueuedRequest.Channel
				);
			FinishPromotedOverlapRequest(SelectedRequestId, Result);
			continue;
		}

		FOpenMobileHapticPlaybackResult Result;
		if (QueuedRequest.QueuedSemanticRequest.IsSet())
		{
			Result = SubmitSemanticOrOverride(
				QueuedRequest.QueuedSemanticRequest.GetValue(),
				QueuedRequest.QueuedSemanticPatternOverride,
				&QueuedRequest.Token
			);
		}
		else if (QueuedRequest.QueuedOneShotRequest.IsSet())
		{
			Result = SubmitOneShotInternal(
				QueuedRequest.QueuedOneShotRequest.GetValue(),
				&QueuedRequest.Token
			);
		}
		else if (QueuedRequest.QueuedNamedRequest.IsSet())
		{
			Result = SubmitNamedPatternInternal(
				QueuedRequest.QueuedNamedRequest.GetValue(),
				&QueuedRequest.Token
			);
		}
		else
		{
			Result =
				OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
					EOpenMobileHapticsFailureReason::Internal,
					EOpenMobileHapticFailureStage::Channel,
					QueuedRequest.Effect,
					QueuedRequest.Channel
				);
		}
		FinishPromotedOverlapRequest(SelectedRequestId, Result);
		if (State)
		{
			const FOpenMobileHapticsSubsystemRequestState* Current =
				State->Requests.Find(SelectedRequestId);
			if (Current && Current->bWaitingForOverlap)
			{
				return;
			}
		}
	}
}

void UOpenMobileHapticsSubsystem::FinishPromotedOverlapRequest(
	uint64 RequestId,
	const FOpenMobileHapticPlaybackResult& Result
)
{
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
	if (Result.IsAccepted())
	{
		if (Request->bWaitingForOverlap)
		{
			return;
		}
		if (!Result.ResolvedPath.IsNone())
		{
			Request->ResolvedPath = Result.ResolvedPath;
		}
		if (Request->bFireAndForgetAfterOverlapPromotion)
		{
			FOpenMobileHapticPlaybackEvent Started;
			Started.State = EOpenMobileHapticPlaybackState::Started;
			Started.Evidence = EOpenMobileHapticEventEvidence::Estimated;
			Started.TimestampSeconds = FPlatformTime::Seconds();
			PublishPlaybackEvent(RequestId, MoveTemp(Started));
			if (State && State->Requests.Contains(RequestId))
			{
				FOpenMobileHapticPlaybackEvent Completed;
				Completed.State = EOpenMobileHapticPlaybackState::Completed;
				Completed.Evidence = EOpenMobileHapticEventEvidence::Estimated;
				Completed.TimestampSeconds = FPlatformTime::Seconds();
				PublishPlaybackEvent(RequestId, MoveTemp(Completed));
			}
		}
		return;
	}
	if (Request->bWaitingForOverlap
		&& Result.Error.Code == EOpenMobileHapticErrorCode::ChannelBusy)
	{
		return;
	}
	Request->ResolvedPath = Result.ResolvedPath.IsNone()
		? FName(TEXT("OverlapQueueRejected"))
		: Result.ResolvedPath;
	Request->FallbackAttempts = Result.FallbackAttempts;
	FOpenMobileHapticPlaybackEvent Event;
	Event.State = Result.Outcome == EOpenMobileHapticPlaybackOutcome::Suppressed
		? EOpenMobileHapticPlaybackState::Cancelled
		: EOpenMobileHapticPlaybackState::Failed;
	Event.Evidence = EOpenMobileHapticEventEvidence::SchedulerConfirmed;
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.Error = Result.Error;
	PublishPlaybackEvent(RequestId, MoveTemp(Event));
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::TrackInitialSubmissionResult(
	FOpenMobileHapticPlaybackResult Result
)
{
	if (State && Result.Outcome == EOpenMobileHapticPlaybackOutcome::Fallback
		&& State->FallbackPlaybackCount != MAX_uint64)
	{
		++State->FallbackPlaybackCount;
	}
	if (State && !Result.IsAccepted())
	{
		State->PerformanceTracker.RecordDroppedRequest();
	}
	return Result;
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitSemantic(
	const FOpenMobileHapticSemanticRequest& Request
)
{
	return TrackInitialSubmissionResult(
		SubmitSemanticOrOverride(Request, NAME_None)
	);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitSemanticOrOverride(
	const FOpenMobileHapticSemanticRequest& Request,
	FName PatternOverride,
	const FOpenMobileHapticsBackendRequestToken* ExistingToken
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_SubmitSemantic);
	check(IsInGameThread());
	const FOpenMobileHapticsSemanticDescriptor Descriptor =
		FOpenMobileHapticsSemanticPolicy::Describe(Request.Effect);
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	FOpenMobileHapticSemanticRequest AdjustedRequest = Request;
	AdjustedRequest.Options.Category =
		OpenMobileHapticsSubsystemPrivate::ResolveCategory(
			Request.Options.Category,
			Descriptor.Category,
			Settings->DefaultCategory
		);
	if (static_cast<uint8>(Request.Effect)
			> static_cast<uint8>(EOpenMobileHapticSemanticEffect::Achievement)
		|| !FMath::IsFinite(Request.Intensity)
		|| Request.Intensity < 0.0f
		|| Request.Intensity > 1.0f
		|| !FMath::IsFinite(Request.Options.IntensityScale)
		|| Request.Options.IntensityScale < 0.0f
		|| Request.Options.IntensityScale > 1.0f
		|| Request.Options.Channel.IsNone()
		|| AdjustedRequest.Options.Category.IsNone()
		|| static_cast<uint8>(Request.Options.Priority)
			> static_cast<uint8>(EOpenMobileHapticChannelPriority::Critical)
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
			Descriptor.Name,
			Request.Options.Channel
		);
	}
	const FOpenMobileHapticsResolvedChannel ResolvedChannel =
		FOpenMobileHapticsChannelPolicy::Resolve(
			Request.Options.Channel,
			Request.Options.Priority,
			Settings->Channels,
			Settings->MaximumActiveHandles,
			Settings->MaximumQueueDepthPerChannel
		);
	AdjustedRequest.Options.Priority = ResolvedChannel.EffectivePriority;
	const bool bAllowedByUserPolicy =
		OpenMobileHapticsSubsystemPrivate::IsAllowedByUserPolicy(
			UserPolicy,
			AdjustedRequest.Options.Priority,
			AdjustedRequest.Options.Category
		);
	if (!bAllowedByUserPolicy)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("PlayerPolicy")
		);
	}
	const float MutablePolicyScale =
		OpenMobileHapticsSubsystemPrivate::UserPolicyScale(
			UserPolicy,
			AdjustedRequest.Options.Priority,
			AdjustedRequest.Options.Category,
			Descriptor.Name
		);
	if (MutablePolicyScale <= 0.0f)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("ZeroIntensity")
		);
	}
	const EOpenMobileHapticsLifecycleRequestOutcome LifecycleOutcome =
		OpenMobileHapticsSubsystemPrivate::EvaluateLifecycle(
			AdjustedRequest.Options,
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
		bAllowedByUserPolicy
	))
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRecoveryPendingPlaybackResult(
			Descriptor.Name,
			Request.Options.Channel
		);
	}

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
		if (Effect.Name == Descriptor.Name)
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
	const bool bCoalescible = !AdjustedRequest.Options.Loop.bLoop
		&& AdjustedRequest.Options.Schedule.Mode
			== EOpenMobileHapticScheduleMode::Immediate
		&& (bSelection || AdjustedRequest.Options.Category == TEXT("UI"));
	const double EquivalentRequestDebounceSeconds = bSelection
		? Settings->SelectionDebounceSeconds
		: bCoalescible
			? Settings->UIRequestDebounceSeconds
			: 0.0;
	const FOpenMobileHapticsRateLimitDecision RateLimitDecision =
		ExistingToken
			? FOpenMobileHapticsRateLimitDecision{}
			: LocalState.RateLimiter.Evaluate(
				OpenMobileHapticsSubsystemPrivate::MakeRateLimitRequest(
					AdjustedRequest.Options,
					Descriptor.Name,
					bCoalescible,
					OpenMobileHapticsSubsystemPrivate::
						MakeSemanticEquivalenceHash(
							AdjustedRequest,
							PatternOverride
						)
				),
				OpenMobileHapticsSubsystemPrivate::ResolveRateLimitPolicy(
					*Settings,
					AdjustedRequest.Options.Channel,
					Descriptor.Name,
					EquivalentRequestDebounceSeconds
				)
			);
	if (!RateLimitDecision.IsAllowed())
	{
		return OpenMobileHapticsSubsystemPrivate::
			MakeRateLimitedPlaybackResult(
				Request.Options.Channel,
				RateLimitDecision.Outcome
			);
	}
	FOpenMobileHapticPlaybackResult OverlapResult;
	bool bShouldQueueForOverlap = false;
	bool bUsedMixFallback = false;
	if (!ResolveAndApplyOverlap(
		AdjustedRequest.Options,
		Descriptor.Name,
		ExistingToken ? ExistingToken->RequestId : 0,
		OverlapResult,
		bShouldQueueForOverlap,
		bUsedMixFallback
	))
	{
		return OverlapResult;
	}
	if (bShouldQueueForOverlap)
	{
		FOpenMobileHapticSemanticRequest QueuedRequest = Request;
		QueuedRequest.Options = AdjustedRequest.Options;
		return QueueOverlapRequest(
			AdjustedRequest.Options,
			ResolvedChannel,
			Descriptor.Name,
			AdjustedRequest.Options.Loop.bLoop,
			bUsedMixFallback,
			ExistingToken,
			&QueuedRequest,
			nullptr,
			nullptr,
			PatternOverride
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
			ExistingToken
				? *ExistingToken
				: FOpenMobileHapticsBackendRegistry::CreateRequestToken(
					*Backend,
					true
				);
		FOpenMobileHapticPlaybackResult AdmissionRejection;
		if (!AdmitChannelRequest(
			OverrideToken,
			AdjustedRequest.Options,
			ResolvedChannel.MaximumActiveHandles,
			ResolvedChannel.MaximumQueueDepth,
			PlaybackParameters.ScheduledStartGuard.IsValid(),
			false,
			AdjustedRequest.Options.Loop.bLoop,
			Descriptor.Name,
			AdmissionRejection
		))
		{
			return AdmissionRejection;
		}
		FOpenMobileHapticsSubsystemRequestState OverrideState;
		OverrideState.Token = OverrideToken;
		OverrideState.Channel = AdjustedRequest.Options.Channel;
		OverrideState.Category = AdjustedRequest.Options.Category;
		OverrideState.Effect = Descriptor.Name;
		OverrideState.Priority = ResolvedChannel.EffectivePriority;
		OverrideState.ResolvedUserPolicyScale = MutablePolicyScale;
		OverrideState.bSupportsDynamicParameters =
			bSupportsDynamicParameters;
		OverrideState.bRequiresPreparedAsset = true;
		OverrideState.ScheduledStartGuard =
			PlaybackParameters.ScheduledStartGuard;
		OpenMobileHapticsSubsystemPrivate::PreserveOverlapQueueLifecycle(
			LocalState,
			OverrideToken.RequestId,
			OverrideState
		);
		LocalState.Requests.Add(OverrideToken.RequestId, OverrideState);
		LocalState.NativeEventDispatcher->RegisterToken(OverrideToken);
		const double NativeSubmissionStartTimeSeconds =
			FPlatformTime::Seconds();
		FOpenMobileHapticsBackendSubmission OverrideSubmission =
			Backend->SubmitNamedPattern(
				NamedRequest,
				PlaybackParameters,
				OverrideToken,
				MakeBackendCallback()
			);
		LocalState.PerformanceTracker.RecordNativeSubmissionLatencySeconds(
			FPlatformTime::Seconds() - NativeSubmissionStartTimeSeconds
		);
		FOpenMobileHapticPlaybackResult OverrideResult =
			OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
				LocalState,
				OverrideToken,
				Request.Options.Channel,
				MoveTemp(OverrideSubmission)
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
				if (bUsedMixFallback)
				{
					OverrideResult.Outcome =
						EOpenMobileHapticPlaybackOutcome::Fallback;
				}
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
		ExistingToken
			? *ExistingToken
			: FOpenMobileHapticsBackendRegistry::CreateRequestToken(
				*Backend,
				bScheduled
			);
	FOpenMobileHapticPlaybackResult AdmissionRejection;
	if (!AdmitChannelRequest(
		Token,
		AdjustedRequest.Options,
		ResolvedChannel.MaximumActiveHandles,
		ResolvedChannel.MaximumQueueDepth,
		bScheduled,
		false,
		AdjustedRequest.Options.Loop.bLoop,
		Descriptor.Name,
		AdmissionRejection
	))
	{
		return AdmissionRejection;
	}
	FOpenMobileHapticsSubsystemRequestState RequestState;
	RequestState.Token = Token;
	RequestState.Channel = AdjustedRequest.Options.Channel;
	RequestState.Category = AdjustedRequest.Options.Category;
	RequestState.Effect = Descriptor.Name;
	RequestState.Priority = ResolvedChannel.EffectivePriority;
	RequestState.ResolvedUserPolicyScale = MutablePolicyScale;
	RequestState.ScheduledStartGuard =
		PlaybackParameters.ScheduledStartGuard;
	OpenMobileHapticsSubsystemPrivate::PreserveOverlapQueueLifecycle(
		LocalState,
		Token.RequestId,
		RequestState
	);
	LocalState.Requests.Add(Token.RequestId, MoveTemp(RequestState));
	LocalState.NativeEventDispatcher->RegisterToken(Token);
	const double NativeSubmissionStartTimeSeconds = FPlatformTime::Seconds();
	FOpenMobileHapticsBackendSubmission Submission = Backend->SubmitSemantic(
		AdjustedRequest,
		Resolution,
		PlaybackParameters,
		Token,
		MakeBackendCallback()
	);
	LocalState.PerformanceTracker.RecordNativeSubmissionLatencySeconds(
		FPlatformTime::Seconds() - NativeSubmissionStartTimeSeconds
	);
	FOpenMobileHapticPlaybackResult Result =
		OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
			LocalState,
			Token,
			Request.Options.Channel,
			MoveTemp(Submission)
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
		if (bUsedMixFallback)
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
	return TrackInitialSubmissionResult(
		SubmitOneShotInternal(Request, nullptr)
	);
}

FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitOneShotInternal(
	const FOpenMobileHapticOneShotRequest& Request,
	const FOpenMobileHapticsBackendRequestToken* ExistingToken
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_SubmitOneShot);
	check(IsInGameThread());
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	FOpenMobileHapticOneShotRequest AdjustedRequest = Request;
	AdjustedRequest.Options.Category =
		OpenMobileHapticsSubsystemPrivate::ResolveCategory(
			Request.Options.Category,
			NAME_None,
			Settings->DefaultCategory
		);
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
		|| AdjustedRequest.Options.Category.IsNone()
		|| static_cast<uint8>(Request.Options.Priority)
			> static_cast<uint8>(EOpenMobileHapticChannelPriority::Critical)
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
		|| Request.Intensity == 0.0f)
	{
		const FName Reason = Request.DurationSeconds == 0.0f
				? FName(TEXT("ZeroDuration"))
				: FName(TEXT("ZeroIntensity"));
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			Reason
		);
	}
	const FOpenMobileHapticsResolvedChannel ResolvedChannel =
		FOpenMobileHapticsChannelPolicy::Resolve(
			Request.Options.Channel,
			Request.Options.Priority,
			Settings->Channels,
			Settings->MaximumActiveHandles,
			Settings->MaximumQueueDepthPerChannel
		);
	AdjustedRequest.Options.Priority = ResolvedChannel.EffectivePriority;
	const bool bAllowedByUserPolicy =
		OpenMobileHapticsSubsystemPrivate::IsAllowedByUserPolicy(
			UserPolicy,
			AdjustedRequest.Options.Priority,
			AdjustedRequest.Options.Category
		);
	if (!bAllowedByUserPolicy)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("PlayerPolicy")
		);
	}
	const float MutablePolicyScale =
		OpenMobileHapticsSubsystemPrivate::UserPolicyScale(
			UserPolicy,
			AdjustedRequest.Options.Priority,
			AdjustedRequest.Options.Category,
			TEXT("OneShot")
		);
	if (MutablePolicyScale <= 0.0f)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("ZeroIntensity")
		);
	}
	if (OpenMobileHapticsSubsystemPrivate::EvaluateLifecycle(
		AdjustedRequest.Options,
		EOpenMobileHapticsLifecycleRequestKind::OneShot
	) == EOpenMobileHapticsLifecycleRequestOutcome::Suppressed)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			OpenMobileHapticsSubsystemPrivate::LifecycleSuppressionReason()
		);
	}
	if (!FOpenMobileHapticsBackendRegistry::RequestRecovery(
		bAllowedByUserPolicy
	))
	{
		return OpenMobileHapticsSubsystemPrivate::MakeRecoveryPendingPlaybackResult(
			TEXT("OneShot"),
			Request.Options.Channel
		);
	}

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
		if (Effect.Name == TEXT("OneShot"))
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
	const FOpenMobileHapticsRateLimitDecision RateLimitDecision =
		ExistingToken
			? FOpenMobileHapticsRateLimitDecision{}
			: LocalState.RateLimiter.Evaluate(
				OpenMobileHapticsSubsystemPrivate::MakeRateLimitRequest(
					AdjustedRequest.Options,
					TEXT("OneShot"),
					false,
					0
				),
				OpenMobileHapticsSubsystemPrivate::ResolveRateLimitPolicy(
					*Settings,
					AdjustedRequest.Options.Channel,
					TEXT("OneShot"),
					0.0
				)
			);
	if (!RateLimitDecision.IsAllowed())
	{
		return OpenMobileHapticsSubsystemPrivate::
			MakeRateLimitedPlaybackResult(
				Request.Options.Channel,
				RateLimitDecision.Outcome
			);
	}
	FOpenMobileHapticPlaybackResult OverlapResult;
	bool bShouldQueueForOverlap = false;
	bool bUsedMixFallback = false;
	if (!ResolveAndApplyOverlap(
		AdjustedRequest.Options,
		TEXT("OneShot"),
		ExistingToken ? ExistingToken->RequestId : 0,
		OverlapResult,
		bShouldQueueForOverlap,
		bUsedMixFallback
	))
	{
		return OverlapResult;
	}
	if (bShouldQueueForOverlap)
	{
		FOpenMobileHapticOneShotRequest QueuedRequest = Request;
		QueuedRequest.Options = AdjustedRequest.Options;
		return QueueOverlapRequest(
			AdjustedRequest.Options,
			ResolvedChannel,
			TEXT("OneShot"),
			AdjustedRequest.Options.Loop.bLoop,
			bUsedMixFallback,
			ExistingToken,
			nullptr,
			&QueuedRequest,
			nullptr
		);
	}
	const FOpenMobileHapticsBackendRequestToken Token =
		ExistingToken
			? *ExistingToken
			: FOpenMobileHapticsBackendRegistry::CreateRequestToken(
				*Backend,
				true
			);
	FOpenMobileHapticsBackendPlaybackParameters PlaybackParameters;
	PlaybackParameters.Timing = Timing;
	PlaybackParameters.ScheduledStartGuard =
		OpenMobileHapticsSubsystemPrivate::MakeScheduledStartGuard(Timing);
	FOpenMobileHapticPlaybackResult AdmissionRejection;
	if (!AdmitChannelRequest(
		Token,
		AdjustedRequest.Options,
		ResolvedChannel.MaximumActiveHandles,
		ResolvedChannel.MaximumQueueDepth,
		PlaybackParameters.ScheduledStartGuard.IsValid(),
		false,
		AdjustedRequest.Options.Loop.bLoop,
		TEXT("OneShot"),
		AdmissionRejection
	))
	{
		return AdmissionRejection;
	}
	FOpenMobileHapticsSubsystemRequestState RequestState;
	RequestState.Token = Token;
	RequestState.Channel = AdjustedRequest.Options.Channel;
	RequestState.Category = AdjustedRequest.Options.Category;
	RequestState.Effect = TEXT("OneShot");
	RequestState.Priority = ResolvedChannel.EffectivePriority;
	RequestState.ResolvedUserPolicyScale = MutablePolicyScale;
	RequestState.ScheduledStartGuard =
		PlaybackParameters.ScheduledStartGuard;
	OpenMobileHapticsSubsystemPrivate::PreserveOverlapQueueLifecycle(
		LocalState,
		Token.RequestId,
		RequestState
	);
	LocalState.Requests.Add(Token.RequestId, MoveTemp(RequestState));
	LocalState.NativeEventDispatcher->RegisterToken(Token);
	const double NativeSubmissionStartTimeSeconds = FPlatformTime::Seconds();
	FOpenMobileHapticsBackendSubmission Submission = Backend->SubmitOneShot(
		AdjustedRequest,
		Resolution,
		PlaybackParameters,
		Token,
		MakeBackendCallback()
	);
	LocalState.PerformanceTracker.RecordNativeSubmissionLatencySeconds(
		FPlatformTime::Seconds() - NativeSubmissionStartTimeSeconds
	);
	FOpenMobileHapticPlaybackResult Result =
		OpenMobileHapticsSubsystemPrivate::FinalizeSubmission(
			LocalState,
			Token,
			Request.Options.Channel,
			MoveTemp(Submission)
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
		if (bUsedMixFallback)
		{
			Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
		}
	}
	PublishSubmissionEvents(Result, Timing);
	return Result;
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitNamedPattern(
	const FOpenMobileHapticNamedPatternRequest& Request
)
{
	return TrackInitialSubmissionResult(
		SubmitNamedPatternInternal(Request, nullptr)
	);
}

#if !UE_BUILD_SHIPPING
FOpenMobileHapticPlaybackResult UOpenMobileHapticsSubsystem::SubmitCookedPreview(
	UOpenMobileHapticPatternAsset* PatternAsset,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	check(IsInGameThread());
	if (!PatternAsset || !PatternAsset->IsDerivedDataCurrent())
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Preview pattern data is invalid.")
		);
	}
	FOpenMobileHapticNamedPatternRequest Request;
	Request.PatternName = TEXT("LivePreview");
	Request.PatternAsset = FSoftObjectPath(PatternAsset);
	Request.Intensity = 1.0f;
	Request.Options = Options;
	return TrackInitialSubmissionResult(
		SubmitNamedPatternInternal(Request, nullptr, true)
	);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitCapabilityTestPattern(
	UOpenMobileHapticPatternAsset* PatternAsset,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	check(IsInGameThread());
	constexpr uint32 MaximumTesterDurationMicroseconds = 500000;
	const FSoftObjectPath PlatformOverride = PatternAsset
		? PatternAsset->GetOverrideForCurrentPlatform()
		: FSoftObjectPath();
	if (!PatternAsset || !PatternAsset->IsDerivedDataCurrent()
		|| PatternAsset->Loop.bLoop
		|| PatternAsset->GetCookedPattern().DurationMicroseconds
			> MaximumTesterDurationMicroseconds
		|| !OpenMobileHapticsSubsystemPrivate::
			IsCapabilityTestPlatformOverrideSafe(PlatformOverride))
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Capability test patterns must be valid, non-looping, and no longer than 0.5 seconds.")
		);
	}
	FOpenMobileHapticNamedPatternRequest Request;
	Request.PatternName = TEXT("CapabilityTester");
	Request.PatternAsset = FSoftObjectPath(PatternAsset);
	Request.PlatformOverrideAsset = PlatformOverride;
	Request.Intensity = 0.65f;
	Request.Options = Options;
	return TrackInitialSubmissionResult(
		SubmitNamedPatternInternal(Request, nullptr, true)
	);
}
#endif

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSubsystem::SubmitNamedPatternInternal(
	const FOpenMobileHapticNamedPatternRequest& Request,
	const FOpenMobileHapticsBackendRequestToken* ExistingToken,
	bool bBypassNamedLibraries
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_SubmitNamedPattern);
	check(IsInGameThread());
	FOpenMobileHapticsSubsystemState& LocalState = GetOrCreateState();
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	FOpenMobileHapticNamedPatternRequest ResolvedRequest = Request;
	FSoftObjectPath LifecyclePatternPath = Request.PatternAsset;
	if (!bBypassNamedLibraries && Settings->NamedLibraries.Num() > 0
		&& LocalState.LibraryResolver.Find(
			Request.PatternName,
			LifecyclePatternPath
		))
	{
		ResolvedRequest.PatternAsset = LifecyclePatternPath;
	}
	else if (!bBypassNamedLibraries && Settings->NamedLibraries.Num() > 0)
	{
		LifecyclePatternPath.Reset();
	}
	const UOpenMobileHapticPatternAsset* LifecyclePattern =
		Cast<UOpenMobileHapticPatternAsset>(
			LifecyclePatternPath.ResolveObject()
		);
	ResolvedRequest.Options.Category =
		OpenMobileHapticsSubsystemPrivate::ResolveCategory(
			Request.Options.Category,
			LifecyclePattern
				? LifecyclePattern->DefaultCategory
				: NAME_None,
			Settings->DefaultCategory
		);
	if (Request.PatternName.IsNone()
		|| !FMath::IsFinite(Request.Intensity)
		|| Request.Intensity < 0.0f
		|| Request.Intensity > 1.0f
		|| !FMath::IsFinite(Request.Options.IntensityScale)
		|| Request.Options.IntensityScale < 0.0f
		|| Request.Options.IntensityScale > 1.0f
		|| Request.Options.Channel.IsNone()
		|| ResolvedRequest.Options.Category.IsNone()
		|| static_cast<uint8>(Request.Options.Priority)
			> static_cast<uint8>(EOpenMobileHapticChannelPriority::Critical)
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
	const FOpenMobileHapticsResolvedChannel ResolvedChannel =
		FOpenMobileHapticsChannelPolicy::Resolve(
			ResolvedRequest.Options.Channel,
			ResolvedRequest.Options.Priority,
			Settings->Channels,
			Settings->MaximumActiveHandles,
			Settings->MaximumQueueDepthPerChannel
		);
	ResolvedRequest.Options.Priority = ResolvedChannel.EffectivePriority;
	const bool bAllowedByUserPolicy =
		OpenMobileHapticsSubsystemPrivate::IsAllowedByUserPolicy(
			UserPolicy,
			ResolvedRequest.Options.Priority,
			ResolvedRequest.Options.Category
		);
	if (!bAllowedByUserPolicy)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("PlayerPolicy")
		);
	}
	const float MutablePolicyScale =
		OpenMobileHapticsSubsystemPrivate::UserPolicyScale(
			UserPolicy,
			ResolvedRequest.Options.Priority,
			ResolvedRequest.Options.Category,
			ResolvedRequest.PatternName
		);
	if (MutablePolicyScale <= 0.0f)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("ZeroIntensity")
		);
	}
	if (OpenMobileHapticsSubsystemPrivate::EvaluateLifecycle(
		ResolvedRequest.Options,
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
		bAllowedByUserPolicy
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
	if (!Request.PatternAsset.IsNull()
		&& !Request.PatternAsset.ResolveObject())
	{
		FOpenMobileHapticPlaybackResult Result =
			OpenMobileHapticsSubsystemPrivate::MakeRejectedPlaybackResult(
				EOpenMobileHapticsFailureReason::InvalidPattern,
				EOpenMobileHapticFailureStage::Preparation,
				Request.PatternName,
				Request.Options.Channel
			);
		LocalState.LastError = Result.Error;
		return Result;
	}

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
	if (StaticIntensity <= 0.0f || MutablePolicyScale <= 0.0f)
	{
		return OpenMobileHapticsSubsystemPrivate::MakeSuppressedPlaybackResult(
			Request.Options.Channel,
			TEXT("ZeroIntensity")
		);
	}
	if (!bBypassNamedLibraries && !Settings->NamedLibraries.IsEmpty())
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
	if (!bBypassNamedLibraries && !Settings->NamedLibraries.IsEmpty()
		&& LocalState.PreparationState
			== EOpenMobileHapticPreparationState::Prepared
		&& Backend->GetPreparationState()
			!= EOpenMobileHapticPreparationState::Prepared)
	{
		TArray<FString> PreparationErrors;
		const double PreparationStartTimeSeconds = FPlatformTime::Seconds();
		const bool bPrepared = PrepareResolvedResources(PreparationErrors, true);
		LocalState.PerformanceTracker.RecordPreparationLatencySeconds(
			FPlatformTime::Seconds() - PreparationStartTimeSeconds
		);
		if (!bPrepared)
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
	const FOpenMobileHapticNamedPatternRequest ReplayRequest = ResolvedRequest;
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
	const FOpenMobileHapticsRateLimitDecision RateLimitDecision =
		ExistingToken
			? FOpenMobileHapticsRateLimitDecision{}
			: LocalState.RateLimiter.Evaluate(
				OpenMobileHapticsSubsystemPrivate::MakeRateLimitRequest(
					ResolvedRequest.Options,
					ResolvedRequest.PatternName,
					false,
					0
				),
				OpenMobileHapticsSubsystemPrivate::ResolveRateLimitPolicy(
					*Settings,
					ResolvedRequest.Options.Channel,
					ResolvedRequest.PatternName,
					0.0
				)
			);
	if (!RateLimitDecision.IsAllowed())
	{
		return OpenMobileHapticsSubsystemPrivate::
			MakeRateLimitedPlaybackResult(
				Request.Options.Channel,
				RateLimitDecision.Outcome
			);
	}
	const UOpenMobileHapticPatternAsset* PortablePattern =
		Cast<UOpenMobileHapticPatternAsset>(
			ResolvedRequest.PatternAsset.ResolveObject()
		);
	bool bRepeating = ResolvedRequest.Options.Loop.bLoop;
	if (PortablePattern)
	{
		FOpenMobileHapticLoopOptions EffectiveLoop = PortablePattern->Loop;
		if (ResolvedRequest.Options.Loop.bLoop)
		{
			EffectiveLoop = ResolvedRequest.Options.Loop;
		}
		bRepeating = EffectiveLoop.bLoop;
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
	FOpenMobileHapticPlaybackResult OverlapResult;
	bool bShouldQueueForOverlap = false;
	bool bUsedMixFallback = false;
	if (!ResolveAndApplyOverlap(
		ResolvedRequest.Options,
		ResolvedRequest.PatternName,
		ExistingToken ? ExistingToken->RequestId : 0,
		OverlapResult,
		bShouldQueueForOverlap,
		bUsedMixFallback
	))
	{
		return OverlapResult;
	}
	if (bShouldQueueForOverlap)
	{
		return QueueOverlapRequest(
			ResolvedRequest.Options,
			ResolvedChannel,
			ResolvedRequest.PatternName,
			bRepeating,
			bUsedMixFallback,
			ExistingToken,
			nullptr,
			nullptr,
			&ReplayRequest
		);
	}

	const FOpenMobileHapticsBackendRequestToken Token =
		ExistingToken
			? *ExistingToken
			: FOpenMobileHapticsBackendRegistry::CreateRequestToken(
				*Backend,
				true
			);
	FOpenMobileHapticPlaybackResult AdmissionRejection;
	if (!AdmitChannelRequest(
		Token,
		ResolvedRequest.Options,
		ResolvedChannel.MaximumActiveHandles,
		ResolvedChannel.MaximumQueueDepth,
		PlaybackParameters.ScheduledStartGuard.IsValid(),
		false,
		bRepeating,
		ResolvedRequest.PatternName,
		AdmissionRejection
	))
	{
		return AdmissionRejection;
	}
	FOpenMobileHapticsSubsystemRequestState RequestState;
	RequestState.Token = Token;
	RequestState.Channel = ResolvedRequest.Options.Channel;
	RequestState.Category = ResolvedRequest.Options.Category;
	RequestState.Effect = ResolvedRequest.PatternName;
	RequestState.Priority = ResolvedChannel.EffectivePriority;
	RequestState.ResolvedUserPolicyScale = MutablePolicyScale;
	RequestState.bSupportsDynamicParameters = bSupportsDynamicParameters;
	RequestState.bRequiresPreparedAsset =
		!bBypassNamedLibraries && !Settings->NamedLibraries.IsEmpty();
	RequestState.RecoveryRequest = ReplayRequest;
	RequestState.ScheduledStartGuard =
		PlaybackParameters.ScheduledStartGuard;
	OpenMobileHapticsSubsystemPrivate::PreserveOverlapQueueLifecycle(
		LocalState,
		Token.RequestId,
		RequestState
	);
	LocalState.Requests.Add(Token.RequestId, RequestState);
	LocalState.NativeEventDispatcher->RegisterToken(Token);
	const double NativeSubmissionStartTimeSeconds = FPlatformTime::Seconds();
	FOpenMobileHapticsBackendSubmission Submission =
		Backend->SubmitNamedPattern(
			ResolvedRequest,
			PlaybackParameters,
			Token,
			MakeBackendCallback()
		);
	LocalState.PerformanceTracker.RecordNativeSubmissionLatencySeconds(
		FPlatformTime::Seconds() - NativeSubmissionStartTimeSeconds
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
		if (bUsedMixFallback)
		{
			Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
		}
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
	if (Request->bWaitingForOverlap)
	{
		const uint64 OwnedRequestId = Request->Token.RequestId;
		CompleteControlledRequest(OwnedRequestId, TerminalState);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
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
	State->DynamicParameterPolicy.CollectReady(
		NowSeconds,
		MinimumIntervalSeconds,
		State->DynamicParameterBatch
	);
	for (const FOpenMobileHapticsScheduledDynamicParameterUpdate& Update :
		State->DynamicParameterBatch)
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

bool UOpenMobileHapticsSubsystem::IsHapticsEnabledNative() const
{
	return bUserPolicyEnabled.Load();
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::SetHapticsEnabledNative(bool bEnabled)
{
	check(IsInGameThread());
	if (UserPolicy.bEnabled == bEnabled)
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
		return Result;
	}
	FOpenMobileHapticUserPolicy Policy = UserPolicy;
	Policy.bEnabled = bEnabled;
	return ApplyUserPolicy(Policy, true);
}

float UOpenMobileHapticsSubsystem::GetMasterIntensityNative() const
{
	return UserPolicy.MasterIntensity;
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSubsystem::SetMasterIntensityNative(float MasterIntensity)
{
	check(IsInGameThread());
	if (FMath::IsFinite(MasterIntensity)
		&& MasterIntensity >= 0.0f
		&& MasterIntensity <= 1.0f
		&& UserPolicy.MasterIntensity == MasterIntensity)
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
		return Result;
	}
	FOpenMobileHapticUserPolicy Policy = UserPolicy;
	Policy.MasterIntensity = MasterIntensity;
	return ApplyUserPolicy(Policy, false);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::UpdateUserPolicy(
	const FOpenMobileHapticUserPolicy& Policy
)
{
	return ApplyUserPolicy(Policy, false);
}

FOpenMobileHapticControlResult UOpenMobileHapticsSubsystem::ApplyUserPolicy(
	const FOpenMobileHapticUserPolicy& Policy,
	bool bPreserveAllowedScheduledStarts
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
	if (OpenMobileHapticsSubsystemPrivate::PoliciesEqual(UserPolicy, Policy))
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
		return Result;
	}

	const FOpenMobileHapticUserPolicy PreviousPolicy = UserPolicy;
	UserPolicy = Policy;
	bUserPolicyEnabled.Store(Policy.bEnabled);
	if (State)
	{
		if (!bPreserveAllowedScheduledStarts)
		{
			OpenMobileHapticsSubsystemPrivate::InvalidateScheduledStarts(*State);
		}
		const UOpenMobileHapticsSettings* Settings =
			GetDefault<UOpenMobileHapticsSettings>();
		State->PendingRecoveryPlaybacks.RemoveAll(
			[&Policy, Settings](
				const FOpenMobileHapticsPendingRecoveryPlayback& Pending
			)
			{
				const FOpenMobileHapticsResolvedChannel ResolvedChannel =
					FOpenMobileHapticsChannelPolicy::Resolve(
						Pending.Request.Options.Channel,
						Pending.Request.Options.Priority,
						Settings->Channels,
						Settings->MaximumActiveHandles,
						Settings->MaximumQueueDepthPerChannel
					);
				const float PolicyScale =
					OpenMobileHapticsSubsystemPrivate::UserPolicyScale(
						Policy,
						ResolvedChannel.EffectivePriority,
						Pending.Request.Options.Category,
						Pending.Request.PatternName
					);
				return PolicyScale <= 0.0f;
			}
		);
		TArray<uint64> QueuedRequestIds;
		TArray<uint64> ScheduledRequestIds;
		TArray<uint64> ActiveRequestIds;
		TArray<uint64> DynamicRequestIds;
		for (TPair<uint64, FOpenMobileHapticsSubsystemRequestState>& Pair :
			State->Requests)
		{
			FOpenMobileHapticsSubsystemRequestState& Request = Pair.Value;
			Request.ResolvedUserPolicyScale =
				OpenMobileHapticsSubsystemPrivate::UserPolicyScale(
					Policy,
					Request.Priority,
					Request.Category,
					Request.Effect
				);
			const bool bProducesFeedback =
				Request.ResolvedUserPolicyScale > 0.0f;
			if (Request.bWaitingForOverlap)
			{
				if (!bProducesFeedback)
				{
					QueuedRequestIds.Add(Pair.Key);
				}
				continue;
			}
			if (Request.SubmissionState
				== EOpenMobileHapticPlaybackState::Scheduled)
			{
				if (!bProducesFeedback || !bPreserveAllowedScheduledStarts)
				{
					ScheduledRequestIds.Add(Pair.Key);
				}
				continue;
			}
			if (Request.bSupportsDynamicParameters)
			{
				DynamicRequestIds.Add(Pair.Key);
			}
			else if (!bProducesFeedback
				&& Request.Token.PlaybackHandle.IsValid())
			{
				ActiveRequestIds.Add(Pair.Key);
			}
		}
		QueuedRequestIds.Sort();
		ScheduledRequestIds.Sort();
		ActiveRequestIds.Sort();
		DynamicRequestIds.Sort();
		for (const uint64 RequestId : QueuedRequestIds)
		{
			if (State && State->Requests.Contains(RequestId))
			{
				CompleteControlledRequest(
					RequestId,
					EOpenMobileHapticPlaybackState::Cancelled
				);
			}
		}
		for (const uint64 RequestId : ScheduledRequestIds)
		{
			FOpenMobileHapticsSubsystemRequestState* Request =
				State ? State->Requests.Find(RequestId) : nullptr;
			if (!Request)
			{
				continue;
			}
			const FOpenMobileHapticPlaybackHandle Handle =
				Request->Token.PlaybackHandle;
			const bool bHasStartGuard = Request->ScheduledStartGuard.IsValid();
			if (Request->ScheduledStartGuard)
			{
				Request->ScheduledStartGuard->Invalidate();
			}
			const FOpenMobileHapticControlResult EndResult = EndPlaybackNative(
				Handle,
				EOpenMobileHapticPlaybackState::Cancelled
			);
			if (EndResult.Outcome != EOpenMobileHapticControlOutcome::Accepted
				&& bHasStartGuard
				&& State
				&& State->Requests.Contains(RequestId))
			{
				CompleteControlledRequest(
					RequestId,
					EOpenMobileHapticPlaybackState::Cancelled
				);
			}
		}
		for (const uint64 RequestId : ActiveRequestIds)
		{
			const FOpenMobileHapticsSubsystemRequestState* Request =
				State ? State->Requests.Find(RequestId) : nullptr;
			if (Request)
			{
				EndPlaybackNative(
					Request->Token.PlaybackHandle,
					EOpenMobileHapticPlaybackState::Stopped
				);
			}
		}
		for (const uint64 RequestId : DynamicRequestIds)
		{
			const FOpenMobileHapticsSubsystemRequestState* Request =
				State ? State->Requests.Find(RequestId) : nullptr;
			if (!Request)
			{
				continue;
			}
			FOpenMobileHapticDynamicParameterUpdate Update;
			Update.Intensity = FOpenMobileHapticsIntensityPolicy::Scale(
				Request->RuntimeIntensity,
				OpenMobileHapticsSubsystemPrivate::ActivePolicyScale(
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
	OnPolicyChanged.Broadcast(PreviousPolicy, UserPolicy);
	if (PreviousPolicy.MasterIntensity != UserPolicy.MasterIntensity)
	{
		OnMasterIntensityChanged.Broadcast(
			PreviousPolicy.MasterIntensity,
			UserPolicy.MasterIntensity
		);
	}
	BroadcastCapabilitiesIfChanged();
	return Result;
}

FOpenMobileHapticsDiagnostics
UOpenMobileHapticsSubsystem::GetDiagnosticsNative() const
{
	FOpenMobileHapticsDiagnostics Diagnostics;
	Diagnostics.Capabilities = GetCapabilitiesNative();
	Diagnostics.ApplicationState =
		OpenMobileHapticsSubsystemPrivate::ApplicationStateName(
			FOpenMobileHapticsBackendRegistry::GetApplicationState()
		);
	Diagnostics.bBackendRecovering =
		FOpenMobileHapticsBackendRegistry::IsRecovering();
	Diagnostics.bBackendShuttingDown =
		FOpenMobileHapticsBackendRegistry::IsShuttingDown();
	const FOpenMobileHapticsTimelineCacheStatistics CacheStatistics =
		FOpenMobileHapticsBackendRegistry::GetTimelineManager().GetStatistics();
	Diagnostics.Performance.TimelineCacheHitCount =
		OpenMobileHapticsSubsystemPrivate::ToPublicCounter(
			CacheStatistics.HitCount
		);
	Diagnostics.Performance.TimelineCacheMissCount =
		OpenMobileHapticsSubsystemPrivate::ToPublicCounter(
			CacheStatistics.MissCount
		);
	Diagnostics.Performance.TimelineCacheEvictionCount =
		OpenMobileHapticsSubsystemPrivate::ToPublicCounter(
			CacheStatistics.EvictionCount
		);
	Diagnostics.Performance.TimelineCacheEntryCount =
		CacheStatistics.EntryCount;
	Diagnostics.Performance.TimelineCacheMemoryBytes =
		CacheStatistics.MemoryBytes;
	Diagnostics.Performance.TimelineCacheMaximumEntryCount =
		CacheStatistics.MaximumEntryCount;
	Diagnostics.Performance.TimelineCacheMaximumMemoryBytes =
		CacheStatistics.MaximumMemoryBytes;
	if (State)
	{
		const FOpenMobileHapticsPerformanceSnapshot Performance =
			State->PerformanceTracker.GetSnapshot();
		Diagnostics.Performance.DroppedRequestCount =
			OpenMobileHapticsSubsystemPrivate::ToPublicCounter(
				Performance.DroppedRequestCount
			);
		Diagnostics.Performance.PeakQueuedPlaybackCount =
			Performance.PeakQueuedPlaybackCount;
		Diagnostics.Performance.PreparationCount =
			OpenMobileHapticsSubsystemPrivate::ToPublicCounter(
				Performance.PreparationCount
			);
		Diagnostics.Performance.LastPreparationLatencyMilliseconds =
			Performance.LastPreparationLatencyMilliseconds;
		Diagnostics.Performance.MaximumPreparationLatencyMilliseconds =
			Performance.MaximumPreparationLatencyMilliseconds;
		Diagnostics.Performance.NativeSubmissionCount =
			OpenMobileHapticsSubsystemPrivate::ToPublicCounter(
				Performance.NativeSubmissionCount
			);
		Diagnostics.Performance.LastNativeSubmissionLatencyMilliseconds =
			Performance.LastNativeSubmissionLatencyMilliseconds;
		Diagnostics.Performance.MaximumNativeSubmissionLatencyMilliseconds =
			Performance.MaximumNativeSubmissionLatencyMilliseconds;
		Diagnostics.ActivePlaybackCount = State->ChannelArbiter.GetActiveCount();
		Diagnostics.QueuedPlaybackCount = State->ChannelArbiter.GetQueuedCount();
		Diagnostics.FallbackPlaybackCount =
			OpenMobileHapticsSubsystemPrivate::ToPublicCounter(
				State->FallbackPlaybackCount
			);
		Diagnostics.bTruncated |= State->ChannelArbiter.BuildDiagnostics(
			Diagnostics.Channels,
			OpenMobileHapticsSubsystemPrivate::MaximumDiagnosticChannelCount
		);
		TArray<uint64> RequestIds;
		State->Requests.GenerateKeyArray(RequestIds);
		RequestIds.Sort();
		for (const uint64 RequestId : RequestIds)
		{
			const FOpenMobileHapticsSubsystemRequestState& Request =
				State->Requests.FindChecked(RequestId);
			if (!Request.Token.PlaybackHandle.IsValid())
			{
				continue;
			}
			if (Diagnostics.ActiveHandles.Num()
				>= OpenMobileHapticsSubsystemPrivate::
					MaximumDiagnosticHandleCount)
			{
				Diagnostics.bTruncated = true;
				break;
			}
			FOpenMobileHapticHandleDiagnostics& Handle =
				Diagnostics.ActiveHandles.AddDefaulted_GetRef();
			Handle.Ordinal = Diagnostics.ActiveHandles.Num();
			Handle.State = Request.LastPublishedState
				!= EOpenMobileHapticPlaybackState::Invalid
					? Request.LastPublishedState
					: Request.SubmissionState;
			Handle.Channel = Request.Channel;
			Handle.PatternOrEffect = Request.Effect;
			Handle.ResolvedPath = Request.ResolvedPath;
			Handle.bQueued = Request.bWaitingForOverlap
				|| Request.QueuedSemanticRequest.IsSet()
				|| Request.QueuedOneShotRequest.IsSet()
				|| Request.QueuedNamedRequest.IsSet();
		}
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
		const int32 MaximumEvents =
			FOpenMobileHapticsBudgetPolicy::ResolveMaximumDiagnosticEvents(
				GetDefault<UOpenMobileHapticsSettings>()->MaximumDiagnosticEvents
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

void UOpenMobileHapticsSubsystem::RegisterPlaybackObject(
	UOpenMobileHapticPlayback* Playback
)
{
	if (!bDeinitialized && IsValid(Playback))
	{
		ActivePlaybackObjects.Add(Playback);
	}
}

void UOpenMobileHapticsSubsystem::UnregisterPlaybackObject(
	UOpenMobileHapticPlayback* Playback
)
{
	ActivePlaybackObjects.Remove(Playback);
}

UOpenMobileHapticPreparationLease*
UOpenMobileHapticsSubsystem::AcquirePreparationLease()
{
	check(IsInGameThread());
	if (bDeinitialized || GetPreparationState()
		!= EOpenMobileHapticPreparationState::Prepared)
	{
		return nullptr;
	}
	for (auto Iterator = ActivePreparationLeases.CreateIterator(); Iterator;
		++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}
	UOpenMobileHapticPreparationLease* Lease =
		NewObject<UOpenMobileHapticPreparationLease>(GetGameInstance());
	Lease->InitializeLease(this);
	ActivePreparationLeases.Add(Lease);
	return Lease;
}

void UOpenMobileHapticsSubsystem::ReleasePreparationLease(
	UOpenMobileHapticPreparationLease* Lease
)
{
	check(IsInGameThread());
	ActivePreparationLeases.Remove(Lease);
	for (auto Iterator = ActivePreparationLeases.CreateIterator(); Iterator;
		++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}
	if (!bDeinitialized && ActivePreparationLeases.IsEmpty()
		&& !bLegacyPreparationClaim)
	{
		ReleaseNamedLibrariesInternal(false);
	}
}

void UOpenMobileHapticsSubsystem::RegisterPreparationAction(
	UOpenMobileHapticPreparationAsyncAction* Action
)
{
	if (!bDeinitialized && IsValid(Action))
	{
		ActivePreparationActions.Add(Action);
	}
}

void UOpenMobileHapticsSubsystem::UnregisterPreparationAction(
	UOpenMobileHapticPreparationAsyncAction* Action
)
{
	ActivePreparationActions.Remove(Action);
}

void UOpenMobileHapticsSubsystem::RegisterNamedPlaybackAction(
	UOpenMobileHapticNamedPlaybackAsyncAction* Action
)
{
	if (!bDeinitialized && IsValid(Action))
	{
		ActiveNamedPlaybackActions.Add(Action);
	}
}

void UOpenMobileHapticsSubsystem::UnregisterNamedPlaybackAction(
	UOpenMobileHapticNamedPlaybackAsyncAction* Action
)
{
	ActiveNamedPlaybackActions.Remove(Action);
}

void UOpenMobileHapticsSubsystem::BroadcastPreparationStateChange(
	EOpenMobileHapticPreparationState PreviousState,
	EOpenMobileHapticPreparationState NewState,
	FString Reason,
	bool bPreparedAssetsRemainLoaded
)
{
	if (PreviousState != NewState)
	{
		OnPreparationStateChanged.Broadcast(
			PreviousState,
			NewState,
			Reason,
			bPreparedAssetsRemainLoaded
		);
	}
}

void UOpenMobileHapticsSubsystem::BroadcastCapabilitiesIfChanged()
{
	const FOpenMobileHapticCapabilities CurrentCapabilities =
		GetCapabilitiesNative();
	if (FOpenMobileHapticCapabilities::StaticStruct()->CompareScriptStruct(
		&LastBroadcastCapabilities,
		&CurrentCapabilities,
		0
	))
	{
		return;
	}
	const FOpenMobileHapticCapabilities PreviousCapabilities =
		LastBroadcastCapabilities;
	LastBroadcastCapabilities = CurrentCapabilities;
	OnCapabilitiesChanged.Broadcast(
		PreviousCapabilities,
		CurrentCapabilities
	);
	if (PreviousCapabilities.Availability
		!= CurrentCapabilities.Availability)
	{
		OnAvailabilityChanged.Broadcast(
			PreviousCapabilities.Availability,
			CurrentCapabilities.Availability
		);
	}
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
	if (!CapabilitiesChangedDelegateHandle.IsValid())
	{
		CapabilitiesChangedDelegateHandle =
			FOpenMobileHapticsBackendRegistry::OnCapabilitiesChanged()
				.AddUObject(
					this,
					&UOpenMobileHapticsSubsystem::HandleCapabilitiesChanged
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
	if (CapabilitiesChangedDelegateHandle.IsValid())
	{
		FOpenMobileHapticsBackendRegistry::OnCapabilitiesChanged().Remove(
			CapabilitiesChangedDelegateHandle
		);
		CapabilitiesChangedDelegateHandle.Reset();
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
	BroadcastCapabilitiesIfChanged();
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
			&& OpenMobileHapticsSubsystemPrivate::ActivePolicyScale(
				*Request
			) > 0.0f
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
	if (State && !State->PendingRecoveryPlaybacks.IsEmpty())
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
	BroadcastCapabilitiesIfChanged();
	TArray<FOpenMobileHapticsPendingRecoveryPlayback> Pending =
		MoveTemp(State->PendingRecoveryPlaybacks);
	State->PendingRecoveryPlaybacks.Reset();
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	if (!Settings->bResumeEligiblePlaybackAfterForeground
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
	TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_ApplicationLifecycle);
	check(IsInGameThread());
	if (bDeinitialized || !State)
	{
		return;
	}
	BroadcastCapabilitiesIfChanged();
	if (Transition.bChanged
		&& Transition.CurrentState
			== EOpenMobileHapticsApplicationState::Active
		&& !GetDefault<UOpenMobileHapticsSettings>()
			->bRetainRateLimitStateAcrossForeground)
	{
		State->RateLimiter.Reset();
	}
	if (!Transition.bInterruptsPlayback)
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

void UOpenMobileHapticsSubsystem::HandleCapabilitiesChanged(
	const FOpenMobileHapticCapabilities& PreviousCapabilities,
	const FOpenMobileHapticCapabilities& NewCapabilities
)
{
	static_cast<void>(PreviousCapabilities);
	static_cast<void>(NewCapabilities);
	if (!bDeinitialized)
	{
		BroadcastCapabilitiesIfChanged();
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
	if (!bDeinitialized)
	{
		EnsureNativeEventDispatcher(*State);
	}
	return *State;
}

void UOpenMobileHapticsSubsystem::EnsureNativeEventDispatcher(
	FOpenMobileHapticsSubsystemState& LocalState
) const
{
	check(IsInGameThread());
	if (LocalState.NativeEventDispatcher)
	{
		return;
	}
	const TWeakObjectPtr<UOpenMobileHapticsSubsystem> WeakSubsystem(
		const_cast<UOpenMobileHapticsSubsystem*>(this)
	);
	LocalState.NativeEventDispatcher = MakeShared<
		FOpenMobileHapticsNativeEventDispatcher,
		ESPMode::ThreadSafe
	>([WeakSubsystem](const FOpenMobileHapticsBackendCallback& Callback)
	{
		check(IsInGameThread());
		if (UOpenMobileHapticsSubsystem* Subsystem = WeakSubsystem.Get())
		{
			Subsystem->HandleBackendCallback(Callback);
		}
	});
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
	const bool bTerminal =
		OpenMobileHapticsSubsystemPrivate::IsTerminalState(Event.State);
	if (bTerminal)
	{
		OpenMobileHapticsSubsystemPrivate::RemoveRequest(*State, RequestId);
	}

	OnPlaybackEvent.Broadcast(Event);
	NativePlaybackEvent.Broadcast(Event);
	if (bTerminal)
	{
		ScheduleOverlapQueueDrain();
	}
}

TFunction<void(const FOpenMobileHapticsBackendCallback&)>
UOpenMobileHapticsSubsystem::MakeBackendCallback()
{
	const TSharedRef<
		FOpenMobileHapticsNativeEventDispatcher,
		ESPMode::ThreadSafe
	> Dispatcher = GetOrCreateState().NativeEventDispatcher.ToSharedRef();
	return [Dispatcher](const FOpenMobileHapticsBackendCallback& Callback)
	{
		Dispatcher->Enqueue(Callback);
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

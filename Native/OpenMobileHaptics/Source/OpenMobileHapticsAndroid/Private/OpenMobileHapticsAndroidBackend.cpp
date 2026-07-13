#include "OpenMobileHapticsAndroidBackend.h"

#include "Android/AndroidPlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsAndroidConfigurationPolicy.h"
#include "OpenMobileHapticsAndroidFallbackPolicy.h"
#include "OpenMobileHapticsAndroidPlaybackControlPolicy.h"
#include "OpenMobileHapticsAndroidWaveformPolicy.h"
#include "OpenMobileHapticsEnvelopePolicy.h"
#include "OpenMobileHapticsFallbackPolicy.h"
#include "OpenMobileHapticsPlatformOverridePolicy.h"
#include "OpenMobileHapticsPrimitiveCompositionPolicy.h"
#include "OpenMobileHapticsSemanticPolicy.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsTimelineManager.h"

void FOpenMobileHapticsAndroidPlaybackControlStore::Add(
	uint64 RequestId,
	FOpenMobileHapticsAndroidControlledPlayback Playback
)
{
	FScopeLock Lock(&Mutex);
	Playbacks.Add(RequestId, MoveTemp(Playback));
}

bool FOpenMobileHapticsAndroidPlaybackControlStore::Find(
	uint64 RequestId,
	FOpenMobileHapticsAndroidControlledPlayback& OutPlayback
) const
{
	FScopeLock Lock(&Mutex);
	const FOpenMobileHapticsAndroidControlledPlayback* Playback =
		Playbacks.Find(RequestId);
	if (!Playback)
	{
		return false;
	}
	OutPlayback = *Playback;
	return true;
}

void FOpenMobileHapticsAndroidPlaybackControlStore::Remove(uint64 RequestId)
{
	FScopeLock Lock(&Mutex);
	Playbacks.Remove(RequestId);
}

void FOpenMobileHapticsAndroidPlaybackControlStore::Reset()
{
	FScopeLock Lock(&Mutex);
	Playbacks.Reset();
}

namespace OpenMobileHapticsAndroidBackendPrivate
{
	constexpr int64 ProbeUnavailable = -1;
	constexpr int64 HasActuator = 1LL << 0;
	constexpr int64 HasSemanticFeedback = 1LL << 1;
	constexpr int64 HasRichHaptics = 1LL << 2;
	constexpr int64 HasAmplitudeControl = 1LL << 3;
	constexpr int64 HasWaveformTiming = 1LL << 5;
	constexpr int64 HasLooping = 1LL << 6;
	constexpr int64 HasPrimitives = 1LL << 7;
	constexpr int64 HasEnvelopes = 1LL << 8;
	constexpr int64 HasFrequencyControl = 1LL << 9;
	constexpr int64 HasPresetKnowledge = 1LL << 10;
	constexpr int64 HasPrimitiveKnowledge = 1LL << 11;
	constexpr int64 HasEnvelopeKnowledge = 1LL << 12;
	constexpr int64 HasFrequencyKnowledge = 1LL << 13;
	constexpr int32 MaximumControlledWaveformSegmentCount = 4096;

	EOpenMobileHapticSupportState SupportFromFlag(
		int64 Flags,
		int64 SupportedFlag,
		int64 KnowledgeFlag = 0
	)
	{
		if (KnowledgeFlag != 0 && (Flags & KnowledgeFlag) == 0)
		{
			return EOpenMobileHapticSupportState::Unknown;
		}
		return (Flags & SupportedFlag) != 0
			? EOpenMobileHapticSupportState::Supported
			: EOpenMobileHapticSupportState::Unsupported;
	}

	EOpenMobileHapticSupportState SupportFromAndroidResult(uint64 Result)
	{
		switch (Result)
		{
		case 1:
			return EOpenMobileHapticSupportState::Supported;
		case 2:
			return EOpenMobileHapticSupportState::Unsupported;
		default:
			return EOpenMobileHapticSupportState::Unknown;
		}
	}

	void AddDetailedSupport(
		FOpenMobileHapticCapabilities& Capabilities,
		const FOpenMobileHapticsAndroidHardwareProbe& Probe
	)
	{
		const bool bPresetKnowledge =
			(Probe.Flags & HasPresetKnowledge) != 0;
		const FName PresetNames[] = {
			TEXT("Tick"),
			TEXT("Click"),
			TEXT("HeavyClick"),
			TEXT("DoubleClick")
		};
		bool bAnyPresetSupported = false;
		bool bAnyPresetUnknown = false;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(PresetNames); ++Index)
		{
			const EOpenMobileHapticSupportState Support = bPresetKnowledge
				? SupportFromAndroidResult(
					(Probe.PresetSupport >> (Index * 2)) & 3ULL
				)
				: EOpenMobileHapticSupportState::Unknown;
			Capabilities.PresetSupport.Emplace(PresetNames[Index], Support);
			bAnyPresetSupported |=
				Support == EOpenMobileHapticSupportState::Supported;
			bAnyPresetUnknown |=
				Support == EOpenMobileHapticSupportState::Unknown;
		}
		Capabilities.PredefinedEffects = bPresetKnowledge
			? bAnyPresetSupported
				? EOpenMobileHapticSupportState::Supported
				: bAnyPresetUnknown
					? EOpenMobileHapticSupportState::Unknown
					: EOpenMobileHapticSupportState::Unsupported
			: EOpenMobileHapticSupportState::Unknown;

		const bool bPrimitiveKnowledge =
			(Probe.Flags & HasPrimitiveKnowledge) != 0;
		const FName PrimitiveNames[] = {
			TEXT("Click"),
			TEXT("Thud"),
			TEXT("Spin"),
			TEXT("QuickRise"),
			TEXT("SlowRise"),
			TEXT("QuickFall"),
			TEXT("Tick"),
			TEXT("LowTick")
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(PrimitiveNames); ++Index)
		{
			const EOpenMobileHapticSupportState Support = bPrimitiveKnowledge
				? (Probe.PrimitiveSupport & (1ULL << Index)) != 0
					? EOpenMobileHapticSupportState::Supported
					: EOpenMobileHapticSupportState::Unsupported
				: EOpenMobileHapticSupportState::Unknown;
			Capabilities.PrimitiveSupport.Emplace(
				PrimitiveNames[Index],
				Support
			);
		}
	}

	int32 PurposeFor(FName Category)
	{
		return Category == TEXT("Alerts")
			? 2
			: Category == TEXT("Gameplay") ? 1 : 0;
	}

	bool IsTerminalPlaybackState(EOpenMobileHapticPlaybackState State)
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

	FOpenMobileHapticControlResult MakeEmulatedControlResult(
		int32 NativeResult,
		const TCHAR* FailureMessage
	)
	{
		FOpenMobileHapticControlResult Result;
		Result.Implementation =
			EOpenMobileHapticControlImplementation::Emulated;
		if (NativeResult == 1)
		{
			Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
			return Result;
		}
		if (NativeResult == 7)
		{
			Result.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
			Result.Error = FOpenMobileHapticError::FromCommon(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The Android portable waveform is no longer active."),
				EOpenMobileHapticFailureStage::Playback
			);
			return Result;
		}
		if (NativeResult == 4)
		{
			Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
			Result.Implementation =
				EOpenMobileHapticControlImplementation::Unsupported;
			Result.Error = FOpenMobileHapticError::FromCommon(
				EOpenMobileErrorCode::NotSupported,
				FailureMessage,
				EOpenMobileHapticFailureStage::Capability
			);
			return Result;
		}
		Result.Error = FOpenMobileHapticError::FromCommon(
			EOpenMobileErrorCode::NativeFailure,
			FailureMessage,
			EOpenMobileHapticFailureStage::Playback
		);
		return Result;
	}

	FOpenMobileHapticsAndroidPlaybackControlResolution ResolveControlledWaveform(
		const FOpenMobileHapticsAndroidControlledPlayback& Playback,
		const FOpenMobileHapticsBackendControlCommand& Command
	)
	{
		if (!Playback.Timeline || !Playback.Timeline->bHasRepeatPlan
			|| Playback.Timeline->AndroidControlBase.Outcome
				!= EOpenMobileHapticsAndroidWaveformOutcome::Ready)
		{
			return {};
		}
		return FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
			Playback.Timeline->AndroidControlBase.TimingsMilliseconds,
			Playback.Timeline->AndroidControlBase.Amplitudes,
			Playback.Timeline->RepeatPlan,
			Command.ResolvedPositionSeconds,
			Command.CompletedRepeatCount,
			Playback.Timeline->RepeatPlan.MaximumDurationSeconds
				- Command.ActiveDurationSeconds,
			MaximumControlledWaveformSegmentCount
		);
	}

	FOpenMobileHapticsBackendSubmission MakeNativeSubmission(
		int32 NativeResult,
		FName ResolvedPath,
		bool bFallback,
		const TCHAR* UnsupportedMessage,
		const TCHAR* FailureMessage
	)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result.ResolvedPath = ResolvedPath;
		switch (NativeResult)
		{
		case 1:
		case 3:
		case 5:
		case 6:
			Submission.Result.Outcome = bFallback
				|| NativeResult == 3 || NativeResult == 5
				? EOpenMobileHapticPlaybackOutcome::Fallback
				: EOpenMobileHapticPlaybackOutcome::Accepted;
			Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
			Submission.bExpectsCallbacks = NativeResult == 6;
			Submission.bCreatesControllablePlayback = NativeResult == 6;
			break;
		case 2:
			Submission.Result.Outcome =
				EOpenMobileHapticPlaybackOutcome::Suppressed;
			Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
			break;
		case 4:
			Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
				EOpenMobileErrorCode::NotSupported,
				UnsupportedMessage
			);
			break;
		default:
			Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
				EOpenMobileErrorCode::NativeFailure,
				FailureMessage
			);
			break;
		}
		return Submission;
	}

	FOpenMobileHapticsBackendSubmission MakeNotConfiguredSubmission()
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("Custom Android vibration was not included in this build.")
		);
		return Submission;
	}

	void AppendAttempts(
		FOpenMobileHapticsBackendSubmission& Submission,
		const TArray<FName>& Attempts
	)
	{
		Submission.Result.FallbackAttempts.Append(Attempts);
	}

	FName Attempt(FName Path, FName Reason)
	{
		return *FString::Printf(
			TEXT("%s:%s"),
			*Path.ToString(),
			*Reason.ToString()
		);
	}

	FOpenMobileHapticsAndroidScheduledPlayback MakeScheduledPlayback(
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		FName PatternOrEffect,
		const FOpenMobileHapticPlaybackOptions& Options,
		FName ResolvedPath,
		const FOpenMobileHapticsBackendEventCallback& Callback
	)
	{
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled;
		Scheduled.ScheduledStartGuard = Parameters.ScheduledStartGuard;
		Scheduled.PatternOrEffect = PatternOrEffect;
		Scheduled.Channel = Options.Channel;
		Scheduled.ResolvedPath = ResolvedPath;
		Scheduled.Callback = Callback;
		if (Parameters.Timing.StartDelaySeconds <= 0.0)
		{
			return Scheduled;
		}
		const double TargetPlatformTimeSeconds =
			Parameters.Timing.Diagnostics.ResolvedPlatformTimeSeconds;
		const double StartDelaySeconds = TargetPlatformTimeSeconds > 0.0
			? FMath::Max(
				0.0,
				TargetPlatformTimeSeconds - FPlatformTime::Seconds()
			)
			: Parameters.Timing.StartDelaySeconds;
		Scheduled.StartDelayMilliseconds = FMath::Clamp<int64>(
			static_cast<int64>(FMath::CeilToDouble(
				StartDelaySeconds * 1000.0
			)),
			1,
			60000
		);
		return Scheduled;
	}

	FOpenMobileHapticsAndroidScheduledPlayback MakeScheduledPlayback(
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticNamedPatternRequest& Request,
		FName ResolvedPath,
		const FOpenMobileHapticsBackendEventCallback& Callback
	)
	{
		return MakeScheduledPlayback(
			Parameters,
			Request.PatternName,
			Request.Options,
			ResolvedPath,
			Callback
		);
	}

	void ApplyBestEffortTiming(
		FOpenMobileHapticsBackendSubmission& Submission,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticPlaybackOptions& Options
	)
	{
		static_cast<void>(Options);
		if (!Submission.Result.IsAccepted()
			|| Parameters.Timing.StartDelaySeconds <= 0.0)
		{
			return;
		}
		Submission.Result.Synchronization = Parameters.Timing.Diagnostics;
		Submission.Result.Synchronization.Mode =
			EOpenMobileHapticSynchronizationMode::BestEffort;
		Submission.Result.Synchronization.EstimatedPrecisionSeconds = FMath::Max(
			0.010,
			Submission.Result.Synchronization.EstimatedPrecisionSeconds
		);
	}

	void ApplyBestEffortTiming(
		FOpenMobileHapticsBackendSubmission& Submission,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticNamedPatternRequest& Request
	)
	{
		ApplyBestEffortTiming(Submission, Parameters, Request.Options);
	}

	FOpenMobileHapticsBackendSubmission SubmitPortableAndFallback(
		FOpenMobileHapticsAndroidBridge& Bridge,
		FOpenMobileHapticsAndroidPlaybackControlStore& PlaybackControlStore,
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendRequestToken& Token,
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		int32 Purpose,
		FOpenMobileHapticsBackendEventCallback Callback,
		TArray<FName> Attempts
	)
	{
		FOpenMobileHapticsAndroidWaveformResolution MissingTimeline;
		MissingTimeline.Outcome =
			EOpenMobileHapticsAndroidWaveformOutcome::FallbackRequired;
		MissingTimeline.Reason = TEXT("MissingManagedTimeline");
		const FOpenMobileHapticsAndroidWaveformResolution& Portable =
			Parameters.PortableTimeline
			&& Parameters.PortableTimeline->Path
				== EOpenMobileHapticsTimelinePath::AndroidWaveform
				? Parameters.PortableTimeline->Android
				: MissingTimeline;
		if (Portable.Outcome
			== EOpenMobileHapticsAndroidWaveformOutcome::Ready)
		{
			const FName ResolvedPath = Portable.bUsesDefaultAmplitude
				? FName(TEXT("AndroidPortableWaveformDefaultAmplitude"))
				: FName(TEXT("AndroidPortableWaveform"));
			const bool bCanEmulateControls =
				Parameters.Timing.StartDelaySeconds <= 0.0
				&& Parameters.PortableTimeline
				&& Parameters.PortableTimeline->bHasRepeatPlan
				&& Parameters.PortableTimeline->AndroidControlBase.Outcome
					== EOpenMobileHapticsAndroidWaveformOutcome::Ready
				&& Callback;
			if (bCanEmulateControls)
			{
				FOpenMobileHapticsBackendControlCommand InitialCommand;
				InitialCommand.State =
					EOpenMobileHapticPlaybackState::Accepted;
				FOpenMobileHapticsAndroidControlledPlayback Controlled;
				Controlled.Timeline = Parameters.PortableTimeline;
				Controlled.Purpose = Purpose;
				const FOpenMobileHapticsAndroidPlaybackControlResolution
					ControlledWaveform = ResolveControlledWaveform(
						Controlled,
						InitialCommand
					);
				if (ControlledWaveform.IsSuccess())
				{
					const uint64 PreparedResourceId =
						ControlledWaveform.TimingsMilliseconds
							== Portable.TimingsMilliseconds
						&& ControlledWaveform.Amplitudes
							== Portable.Amplitudes
						&& ControlledWaveform.RepeatIndex
							== Portable.RepeatIndex
							? Parameters.PortableTimeline->ResourceId
							: 0;
					FOpenMobileHapticsBackendEventCallback ControlledCallback =
						[&PlaybackControlStore,
						 RequestId = Token.RequestId,
						 Callback](
							const FOpenMobileHapticsBackendCallback& Event
						) mutable
						{
							if (IsTerminalPlaybackState(Event.Event.State))
							{
								PlaybackControlStore.Remove(RequestId);
							}
							if (Callback)
							{
								Callback(Event);
							}
						};
					PlaybackControlStore.Add(
						Token.RequestId,
						Controlled
					);
					const int32 ControlledResult =
						Bridge.PlayControlledWaveform(
							Token,
							PreparedResourceId,
							ControlledWaveform.TimingsMilliseconds,
							ControlledWaveform.Amplitudes,
							ControlledWaveform.RepeatIndex,
							Purpose,
							ControlledWaveform.CompletionDurationMilliseconds,
							MakeScheduledPlayback(
								Parameters,
								Request,
								ResolvedPath,
								ControlledCallback
							)
						);
					if (ControlledResult != 4)
					{
						if (ControlledResult != 6)
						{
							PlaybackControlStore.Remove(Token.RequestId);
						}
						FOpenMobileHapticsBackendSubmission Submission =
							MakeNativeSubmission(
								ControlledResult,
								ResolvedPath,
								true,
								TEXT("Android rejected the controllable portable waveform."),
								TEXT("Android could not submit the controllable portable waveform.")
							);
						if (ControlledResult == 6)
						{
							Submission.PlaybackControlSupport.PauseImplementation =
								EOpenMobileHapticControlImplementation::Emulated;
							Submission.PlaybackControlSupport.ResumeImplementation =
								EOpenMobileHapticControlImplementation::Emulated;
							Submission.PlaybackControlSupport.SeekImplementation =
								EOpenMobileHapticControlImplementation::Emulated;
							Submission.PlaybackControlSupport.SeekGranularitySeconds =
								0.001;
							Submission.PlaybackControlSupport.RepeatPlan =
								Parameters.PortableTimeline->RepeatPlan;
							Submission.PlaybackControlSupport.bHasRepeatPlan = true;
						}
						if (Portable.bUsesDefaultAmplitude)
						{
							Submission.Result.Intensity.bNativeClamped = true;
							Attempts.Add(TEXT("AmplitudeControl:Default"));
						}
						AppendAttempts(Submission, Attempts);
						ApplyBestEffortTiming(
							Submission,
							Parameters,
							Request
						);
						return Submission;
					}
					PlaybackControlStore.Remove(Token.RequestId);
					Attempts.Add(TEXT("PlaybackControls:NativeSupportChanged"));
				}
				else
				{
					Attempts.Add(TEXT("PlaybackControls:SegmentLimit"));
				}
			}
			const int32 NativeResult = Bridge.PlayWaveform(
				Token,
				Parameters.PortableTimeline
					? Parameters.PortableTimeline->ResourceId
					: 0,
				Portable.TimingsMilliseconds,
				Portable.Amplitudes,
				Portable.RepeatIndex,
				Purpose,
				MakeScheduledPlayback(
					Parameters,
					Request,
					ResolvedPath,
					Callback
				)
			);
			if (NativeResult != 4)
			{
				FOpenMobileHapticsBackendSubmission Submission =
					MakeNativeSubmission(
						NativeResult,
						ResolvedPath,
						true,
						TEXT("Android rejected the portable waveform."),
						TEXT("Android could not submit the portable waveform.")
					);
				if (Portable.bUsesDefaultAmplitude || NativeResult == 5)
				{
					Submission.Result.Intensity.bNativeClamped = true;
					Attempts.Add(TEXT("AmplitudeControl:Default"));
				}
				AppendAttempts(Submission, Attempts);
				ApplyBestEffortTiming(Submission, Parameters, Request);
				return Submission;
			}
			Attempts.Add(TEXT("PortableRich:NativeSupportChanged"));
		}
		else
		{
			Attempts.Add(Attempt(TEXT("PortableRich"), Portable.Reason));
			if (Portable.Reason == TEXT("InvalidIntensity"))
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result =
					FOpenMobileHapticPlaybackResult::MakeRejected(
						EOpenMobileErrorCode::InvalidArgument,
						TEXT("The portable Android pattern intensity is invalid.")
					);
				AppendAttempts(Submission, Attempts);
				return Submission;
			}
		}

		bool bAllowPrimitive = true;
		bool bAllowPredefined = true;
		for (int32 Retry = 0; Retry < 3; ++Retry)
		{
			const FOpenMobileHapticsAndroidFallbackResolution Fallback =
				FOpenMobileHapticsAndroidFallbackPolicy::Resolve(
					Pattern,
					Capabilities,
					Request.Options.FallbackPolicy,
					bAllowPrimitive,
					bAllowPredefined
				);
			Attempts.Append(Fallback.Attempts);
			if (Fallback.Outcome
				== EOpenMobileHapticsAndroidFallbackOutcome::Primitive)
			{
				const FName ResolvedPath(TEXT("AndroidPrimitiveFallback"));
				const int32 NativeResult = Bridge.PlayPrimitives(
					Token,
					{Fallback.Primitive},
					{Request.Intensity},
					{0},
					Purpose,
					MakeScheduledPlayback(
						Parameters,
						Request,
						ResolvedPath,
						Callback
					)
				);
				if (NativeResult == 4)
				{
					bAllowPrimitive = false;
					Attempts.Add(TEXT("Primitive:NativeSupportChanged"));
					continue;
				}
				FOpenMobileHapticsBackendSubmission Submission =
					MakeNativeSubmission(
						NativeResult,
						ResolvedPath,
						true,
						TEXT("Android rejected the declared primitive fallback."),
						TEXT("Android could not submit the primitive fallback.")
					);
				AppendAttempts(Submission, Attempts);
				ApplyBestEffortTiming(Submission, Parameters, Request);
				return Submission;
			}
			if (Fallback.Outcome
				== EOpenMobileHapticsAndroidFallbackOutcome::Predefined)
			{
				const FName ResolvedPath(TEXT("AndroidPredefinedFallback"));
				const int32 NativeResult = Bridge.PlayPredefined(
					Token,
					static_cast<int32>(Fallback.PredefinedEffect),
					Purpose,
					MakeScheduledPlayback(
						Parameters,
						Request,
						ResolvedPath,
						Callback
					)
				);
				if (NativeResult == 4)
				{
					bAllowPredefined = false;
					Attempts.Add(TEXT("Predefined:NativeSupportChanged"));
					continue;
				}
				FOpenMobileHapticsBackendSubmission Submission =
					MakeNativeSubmission(
						NativeResult,
						ResolvedPath,
						true,
						TEXT("Android rejected the declared predefined fallback."),
						TEXT("Android could not submit the predefined fallback.")
					);
				AppendAttempts(Submission, Attempts);
				ApplyBestEffortTiming(Submission, Parameters, Request);
				return Submission;
			}
			if (Fallback.Outcome
				== EOpenMobileHapticsAndroidFallbackOutcome::Semantic)
			{
				const FOpenMobileHapticsSemanticDescriptor Descriptor =
					FOpenMobileHapticsSemanticPolicy::Describe(
						Fallback.SemanticEffect
					);
				const FOpenMobileHapticsAndroidScheduledPlayback Scheduled =
					MakeScheduledPlayback(
						Parameters,
						Request,
						TEXT("AndroidSemanticFallback"),
						Callback
					);
				const FOpenMobileHapticsAndroidBridgeSubmission Native =
					Bridge.PlaySemantic(
						Token,
						Descriptor.Behavior,
						Request.Intensity,
						EOpenMobileHapticsSemanticPath::SystemSemantic,
						Purpose,
						Scheduled.StartDelayMilliseconds,
						Scheduled.ScheduledStartGuard,
						Descriptor.Name,
						Request.Options.Channel,
						TEXT("AndroidSemanticFallback"),
						MoveTemp(Callback)
					);
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.bExpectsCallbacks = Native.bExpectsCallback;
				Submission.Result.ResolvedPath =
					TEXT("AndroidSemanticFallback");
				if (Native.Result == 1 || Native.Result == 3
					|| Native.Result == 6)
				{
					Submission.Result.Outcome =
						EOpenMobileHapticPlaybackOutcome::Fallback;
					Submission.Result.State =
						EOpenMobileHapticPlaybackState::Accepted;
				}
				else if (Native.Result == 2)
				{
					Submission.Result.Outcome =
						EOpenMobileHapticPlaybackOutcome::Suppressed;
					Submission.Result.State =
						EOpenMobileHapticPlaybackState::Completed;
				}
				else
				{
					Submission.Result =
						FOpenMobileHapticPlaybackResult::MakeRejected(
							EOpenMobileErrorCode::NativeFailure,
							TEXT("Android could not submit the semantic fallback.")
						);
				}
				AppendAttempts(Submission, Attempts);
				ApplyBestEffortTiming(Submission, Parameters, Request);
				return Submission;
			}
			if (Fallback.Outcome
				== EOpenMobileHapticsAndroidFallbackOutcome::BasicVibration)
			{
				const int64 DurationMilliseconds = FMath::Max<int64>(
					1,
					static_cast<int64>(FMath::RoundToDouble(
						static_cast<double>(Pattern.GetCookedPattern()
							.DurationMicroseconds) / 1000.0
					))
				);
				const int32 NativeResult = Bridge.PlayOneShot(
					Token,
					DurationMilliseconds,
					Request.Intensity,
					EOpenMobileHapticsOneShotPath::BasicVibration,
					Purpose,
					MakeScheduledPlayback(
						Parameters,
						Request,
						TEXT("AndroidBasicVibrationFallback"),
						Callback
					)
				);
				if (NativeResult == 4
					&& (Request.Options.FallbackPolicy
							== EOpenMobileHapticFallbackPolicy::NoEffectAllowed
						|| Pattern.FallbackPolicy
							== EOpenMobileHapticFallbackPolicy::NoEffectAllowed))
				{
					FOpenMobileHapticsBackendSubmission Submission;
					Submission.Result.Outcome =
						EOpenMobileHapticPlaybackOutcome::Suppressed;
					Submission.Result.State =
						EOpenMobileHapticPlaybackState::Completed;
					Submission.Result.ResolvedPath = TEXT("NoEffect");
					Attempts.Add(TEXT("NoEffect:Selected"));
					AppendAttempts(Submission, Attempts);
					return Submission;
				}
				FOpenMobileHapticsBackendSubmission Submission =
					MakeNativeSubmission(
						NativeResult,
						NativeResult == 5
							? FName(TEXT("AndroidBasicVibrationDefaultAmplitude"))
							: FName(TEXT("AndroidBasicVibrationFallback")),
						true,
						TEXT("Android rejected the basic vibration fallback."),
						TEXT("Android could not submit the basic vibration fallback.")
					);
				AppendAttempts(Submission, Attempts);
				ApplyBestEffortTiming(Submission, Parameters, Request);
				return Submission;
			}
			if (Fallback.Outcome
				== EOpenMobileHapticsAndroidFallbackOutcome::NoEffect)
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Suppressed;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Completed;
				Submission.Result.ResolvedPath = TEXT("NoEffect");
				AppendAttempts(Submission, Attempts);
				return Submission;
			}
			break;
		}

		if (!GetDefault<UOpenMobileHapticsSettings>()
			->bEnableAndroidCustomVibration)
		{
			FOpenMobileHapticsBackendSubmission Submission =
				MakeNotConfiguredSubmission();
			AppendAttempts(Submission, Attempts);
			return Submission;
		}
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Android pattern and its declared fallbacks are unavailable.")
		);
		AppendAttempts(Submission, Attempts);
		return Submission;
	}
}

bool FOpenMobileHapticsAndroidBackend::IsCustomPlaybackConfigured() const
{
	return GetDefault<UOpenMobileHapticsSettings>()
		->bEnableAndroidCustomVibration;
}

FOpenMobileHapticsBackendControlSupport
FOpenMobileHapticsAndroidBackend::GetControlSupport() const
{
	FOpenMobileHapticsBackendControlSupport Support;
	Support.bStop = true;
	Support.bStopAll = IsCustomPlaybackConfigured();
	Support.bPause = IsCustomPlaybackConfigured();
	Support.bResume = IsCustomPlaybackConfigured();
	Support.bSeek = IsCustomPlaybackConfigured();
	return Support;
}

FOpenMobileHapticCapabilities
FOpenMobileHapticsAndroidBackend::ProbeHardwareCapabilities() const
{
	using namespace OpenMobileHapticsAndroidBackendPrivate;
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.BackendName = GetBackendName();
	Capabilities.Mixing = EOpenMobileHapticSupportState::Unsupported;
	const FOpenMobileHapticsAndroidHardwareProbe Probe = Bridge.QueryHardware();
	if (Probe.Flags == ProbeUnavailable)
	{
		Capabilities.Availability =
			EOpenMobileHapticAvailability::TemporarilyUnavailable;
		Capabilities.Detail =
			TEXT("Android's vibrator service is temporarily unavailable.");
		return Capabilities;
	}
	if ((Probe.Flags & HasActuator) == 0)
	{
		Capabilities.Availability = EOpenMobileHapticAvailability::NoActuator;
		Capabilities.BasicVibration = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.SemanticFeedback = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.RichHaptics = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.AmplitudeControl =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.SemanticEffects =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.PredefinedEffects =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.WaveformTiming =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Looping = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Primitives = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Envelopes = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.FrequencyControl =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.TransientEvents =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.ContinuousEvents =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.DynamicParameters =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.AudioEvents = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.AHAP = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Scheduling = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.BackgroundAlerts =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Pause = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Resume = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Seek = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Detail = TEXT("Android reports no phone vibrator.");
		return Capabilities;
	}

	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	Capabilities.SemanticFeedback =
		(Probe.Flags & HasSemanticFeedback) != 0
			? EOpenMobileHapticSupportState::Supported
			: EOpenMobileHapticSupportState::Unsupported;
	Capabilities.RichHaptics = (Probe.Flags & HasRichHaptics) != 0
		? EOpenMobileHapticSupportState::Supported
		: EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Availability = (Probe.Flags & HasRichHaptics) != 0
		? EOpenMobileHapticAvailability::RichHaptics
		: (Probe.Flags & HasSemanticFeedback) != 0
			? EOpenMobileHapticAvailability::SemanticFeedback
			: EOpenMobileHapticAvailability::BasicVibration;
	Capabilities.AmplitudeControl = SupportFromFlag(
		Probe.Flags,
		HasAmplitudeControl
	);
	Capabilities.SemanticEffects = Capabilities.SemanticFeedback;
	Capabilities.WaveformTiming = SupportFromFlag(
		Probe.Flags,
		HasWaveformTiming
	);
	Capabilities.Looping = SupportFromFlag(Probe.Flags, HasLooping);
	Capabilities.Primitives = SupportFromFlag(
		Probe.Flags,
		HasPrimitives,
		HasPrimitiveKnowledge
	);
	Capabilities.Envelopes = SupportFromFlag(
		Probe.Flags,
		HasEnvelopes,
		HasEnvelopeKnowledge
	);
	Capabilities.FrequencyControl = SupportFromFlag(
		Probe.Flags,
		HasFrequencyControl,
		HasFrequencyKnowledge
	);
	Capabilities.TransientEvents = EOpenMobileHapticSupportState::Supported;
	Capabilities.ContinuousEvents = Capabilities.WaveformTiming;
	Capabilities.DynamicParameters =
		EOpenMobileHapticSupportState::Unsupported;
	Capabilities.AudioEvents = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.AHAP = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Scheduling = EOpenMobileHapticSupportState::Supported;
	Capabilities.BackgroundAlerts =
		EOpenMobileHapticSupportState::Supported;
	AddDetailedSupport(Capabilities, Probe);
	if (Probe.MaximumControlPointCount >= 0
		&& Probe.MaximumControlPointCount <= MAX_int32)
	{
		Capabilities.MaximumControlPointCount = {
			true,
			static_cast<int32>(Probe.MaximumControlPointCount)
		};
	}
	if (Probe.MaximumDurationMillis >= 0)
	{
		Capabilities.MaximumDurationSeconds = {
			true,
			static_cast<double>(Probe.MaximumDurationMillis) / 1000.0
		};
	}
	if (Probe.MinimumTimingMillis >= 0)
	{
		Capabilities.MinimumTimingGranularitySeconds = {
			true,
			static_cast<double>(Probe.MinimumTimingMillis) / 1000.0
		};
	}
	if (Probe.MaximumControlPointDurationMillis >= 0)
	{
		Capabilities.MaximumControlPointDurationSeconds = {
			true,
			static_cast<double>(Probe.MaximumControlPointDurationMillis)
				/ 1000.0
		};
	}
	if (Probe.MinimumFrequencyMilliHertz > 0
		&& Probe.MaximumFrequencyMilliHertz
			>= Probe.MinimumFrequencyMilliHertz)
	{
		Capabilities.FrequencyRange = {
			true,
			static_cast<float>(Probe.MinimumFrequencyMilliHertz) / 1000.0f,
			static_cast<float>(Probe.MaximumFrequencyMilliHertz) / 1000.0f
		};
	}
	Capabilities.Detail = TEXT("Android vibrator capabilities were queried without playback.");
	FOpenMobileHapticsAndroidConfigurationPolicy::ApplyCapabilityMask(
		IsCustomPlaybackConfigured(),
		Capabilities
	);
	Capabilities.Pause = Capabilities.WaveformTiming;
	Capabilities.Resume = Capabilities.WaveformTiming;
	Capabilities.Seek = Capabilities.WaveformTiming;
	return Capabilities;
}

FOpenMobileHapticCapabilities
FOpenMobileHapticsAndroidBackend::GetCapabilities() const
{
	FScopeLock Lock(&CacheMutex);
	if (StableCapabilities.IsSet())
	{
		return StableCapabilities.GetValue();
	}
	FOpenMobileHapticCapabilities Capabilities = ProbeHardwareCapabilities();
	if (Capabilities.Availability
		!= EOpenMobileHapticAvailability::TemporarilyUnavailable)
	{
		StableCapabilities = Capabilities;
	}
	return Capabilities;
}

EOpenMobileHapticPreparationState
FOpenMobileHapticsAndroidBackend::GetPreparationState() const
{
	FScopeLock Lock(&PreparationMutex);
	return PreparationState;
}

FOpenMobileHapticsBackendPreparationResult
FOpenMobileHapticsAndroidBackend::PrepareResources(
	const FOpenMobileHapticsBackendPreparationRequest& Request
)
{
	{
		FScopeLock Lock(&PreparationMutex);
		PreparationState = EOpenMobileHapticPreparationState::Preparing;
	}
	FOpenMobileHapticsBackendPreparationResult Result;
	const FOpenMobileHapticCapabilities Capabilities = GetCapabilities();
	if (Capabilities.Availability
		== EOpenMobileHapticAvailability::TemporarilyUnavailable)
	{
		Result.Errors.Add(TEXT("Android's vibrator service is unavailable during preparation."));
		FScopeLock Lock(&PreparationMutex);
		PreparationState = EOpenMobileHapticPreparationState::Failed;
		return Result;
	}

	for (const TSharedPtr<
		const FOpenMobileHapticsPortableTimeline,
		ESPMode::ThreadSafe
	>& Timeline : Request.Patterns)
	{
		if (!Timeline
			|| Timeline->Path
				!= EOpenMobileHapticsTimelinePath::AndroidWaveform
			|| Timeline->Android.Outcome
				!= EOpenMobileHapticsAndroidWaveformOutcome::Ready)
		{
			continue;
		}
		const int32 NativeResult = Bridge.PrepareWaveform(
			Timeline->ResourceId,
			Timeline->Android.TimingsMilliseconds,
			Timeline->Android.Amplitudes,
			Timeline->Android.RepeatIndex,
			Timeline->EstimatedBytes,
			Request.Limits
		);
		if (NativeResult != 1)
		{
			Result.Errors.Add(TEXT("Android could not compile a prepared waveform."));
			Bridge.ReleasePreparedResources();
			FScopeLock Lock(&PreparationMutex);
			PreparationState = EOpenMobileHapticPreparationState::Failed;
			return Result;
		}
	}

	Result.State = EOpenMobileHapticPreparationState::Prepared;
	FScopeLock Lock(&PreparationMutex);
	PreparationState = Result.State;
	return Result;
}

void FOpenMobileHapticsAndroidBackend::ReleasePreparedResources()
{
	Bridge.ReleasePreparedResources();
	FScopeLock Lock(&PreparationMutex);
	PreparationState = EOpenMobileHapticPreparationState::Unprepared;
}

void FOpenMobileHapticsAndroidBackend::HandleLifecycleChange()
{
	Bridge.StopAll();
	PlaybackControlStore.Reset();
	{
		FScopeLock Lock(&CacheMutex);
		StableCapabilities.Reset();
	}
	ReleasePreparedResources();
}

void FOpenMobileHapticsAndroidBackend::HandleApplicationLifecycle(
	const FOpenMobileHapticsLifecycleTransition& Transition
)
{
	if (Transition.bInterruptsPlayback)
	{
		HandleLifecycleChange();
	}
	if (Transition.bRefreshesNativeServices)
	{
		FScopeLock Lock(&CacheMutex);
		StableCapabilities.Reset();
	}
}

void FOpenMobileHapticsAndroidBackend::HandleInterruption(
	EOpenMobileHapticsInterruptionReason Reason
)
{
	static_cast<void>(Reason);
	Bridge.StopAll();
	PlaybackControlStore.Reset();
	{
		FScopeLock Lock(&CacheMutex);
		StableCapabilities.Reset();
	}
	ReleasePreparedResources();
}

EOpenMobileHapticsRecoveryResult
FOpenMobileHapticsAndroidBackend::RecoverFromInterruption()
{
	const FOpenMobileHapticCapabilities Capabilities =
		ProbeHardwareCapabilities();
	return Capabilities.Availability
		== EOpenMobileHapticAvailability::TemporarilyUnavailable
		? EOpenMobileHapticsRecoveryResult::RetryableFailure
		: EOpenMobileHapticsRecoveryResult::Recovered;
}

void FOpenMobileHapticsAndroidBackend::BeginShutdown()
{
	Bridge.StopAll();
	PlaybackControlStore.Reset();
	ReleasePreparedResources();
	Bridge.Shutdown();
}

FOpenMobileHapticsBackendSubmission
FOpenMobileHapticsAndroidBackend::SubmitSemantic(
	const FOpenMobileHapticSemanticRequest& Request,
	const FOpenMobileHapticsSemanticResolution& Resolution,
	const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
	const FOpenMobileHapticsBackendRequestToken& Token,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	FOpenMobileHapticsBackendSubmission Submission;
	if (Resolution.Path != EOpenMobileHapticsSemanticPath::SystemSemantic
		&& !IsCustomPlaybackConfigured())
	{
		return OpenMobileHapticsAndroidBackendPrivate::
			MakeNotConfiguredSubmission();
	}
	const FOpenMobileHapticsSemanticDescriptor Descriptor =
		FOpenMobileHapticsSemanticPolicy::Describe(Request.Effect);
	const int32 Purpose = Request.Options.Category == TEXT("Alerts")
		? 2
		: Request.Options.Category == TEXT("Gameplay")
			? 1
			: 0;
	const FOpenMobileHapticCapabilities Capabilities = GetCapabilities();
	const bool bAllowBasic = Request.Options.FallbackPolicy
		!= EOpenMobileHapticFallbackPolicy::NoBasicVibration
		&& Request.Options.FallbackPolicy
			!= EOpenMobileHapticFallbackPolicy::ExactOnly;
	EOpenMobileHapticsSemanticPath SubmittedPath = Resolution.Path;
	bool bUsedFallback = Resolution.bFallback;
	if (SubmittedPath == EOpenMobileHapticsSemanticPath::PredefinedEffect)
	{
		const EOpenMobileHapticAndroidPredefinedEffect PredefinedEffect =
			FOpenMobileHapticsAndroidFallbackPolicy::PredefinedForSemantic(
				Descriptor.Behavior
			);
		if (!FOpenMobileHapticsAndroidFallbackPolicy::SupportsPredefined(
			PredefinedEffect,
			Capabilities
		))
		{
			if (bAllowBasic
				&& Capabilities.BasicVibration
					== EOpenMobileHapticSupportState::Supported)
			{
				SubmittedPath = EOpenMobileHapticsSemanticPath::BasicVibration;
			}
			else if (Resolution.bSuppressWhenUnavailable)
			{
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Suppressed;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Completed;
				Submission.Result.ResolvedPath = TEXT("NoEffect");
				return Submission;
			}
			else
			{
				Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
					EOpenMobileErrorCode::NotSupported,
					TEXT("The requested Android predefined effect is unavailable.")
				);
				return Submission;
			}
		}
	}
	FName ResolvedPath =
		FOpenMobileHapticsSemanticPolicy::PathName(SubmittedPath);
	const FOpenMobileHapticsAndroidScheduledPlayback Scheduled =
		OpenMobileHapticsAndroidBackendPrivate::MakeScheduledPlayback(
			Parameters,
			Descriptor.Name,
			Request.Options,
			ResolvedPath,
			Callback
		);
	FOpenMobileHapticsBackendEventCallback RetryCallback = Callback;
	FOpenMobileHapticsAndroidBridgeSubmission BridgeSubmission =
		Bridge.PlaySemantic(
		Token,
		Descriptor.Behavior,
		Request.Intensity,
		SubmittedPath,
		Purpose,
		Scheduled.StartDelayMilliseconds,
		Scheduled.ScheduledStartGuard,
		Descriptor.Name,
		Request.Options.Channel,
		ResolvedPath,
		MoveTemp(Callback)
	);
	if (SubmittedPath == EOpenMobileHapticsSemanticPath::PredefinedEffect
		&& BridgeSubmission.Result == 4
		&& bAllowBasic
		&& Capabilities.BasicVibration
			== EOpenMobileHapticSupportState::Supported)
	{
		SubmittedPath = EOpenMobileHapticsSemanticPath::BasicVibration;
		ResolvedPath = FOpenMobileHapticsSemanticPolicy::PathName(SubmittedPath);
		BridgeSubmission = Bridge.PlaySemantic(
			Token,
			Descriptor.Behavior,
			Request.Intensity,
			SubmittedPath,
			Purpose,
			Scheduled.StartDelayMilliseconds,
			Scheduled.ScheduledStartGuard,
			Descriptor.Name,
			Request.Options.Channel,
			ResolvedPath,
			MoveTemp(RetryCallback)
		);
		bUsedFallback = true;
	}
	if (BridgeSubmission.Result == 4 && Resolution.bSuppressWhenUnavailable)
	{
		BridgeSubmission.Result = 2;
		ResolvedPath = TEXT("NoEffect");
	}
	Submission.Result.ResolvedPath = ResolvedPath;
	Submission.bExpectsCallbacks = BridgeSubmission.bExpectsCallback;
	Submission.bCreatesControllablePlayback =
		BridgeSubmission.bExpectsCallback;
	switch (BridgeSubmission.Result)
	{
	case 1:
		Submission.Result.Outcome = bUsedFallback
			? EOpenMobileHapticPlaybackOutcome::Fallback
			: EOpenMobileHapticPlaybackOutcome::Accepted;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
		break;
	case 2:
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Suppressed;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
		break;
	case 3:
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
		break;
	case 4:
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Android device has no available vibration path.")
		);
		break;
	case 6:
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Accepted;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
		break;
	default:
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not submit semantic Haptics feedback.")
		);
		break;
	}
	OpenMobileHapticsAndroidBackendPrivate::ApplyBestEffortTiming(
		Submission,
		Parameters,
		Request.Options
	);
	return Submission;
}

FOpenMobileHapticControlResult FOpenMobileHapticsAndroidBackend::StopPlayback(
	const FOpenMobileHapticsBackendRequestToken& Token
)
{
	FOpenMobileHapticsAndroidControlledPlayback Controlled;
	if (PlaybackControlStore.Find(Token.RequestId, Controlled))
	{
		const bool bStopped = Bridge.StopControlledWaveform(Token.RequestId);
		PlaybackControlStore.Remove(Token.RequestId);
		if (bStopped)
		{
			FOpenMobileHapticControlResult Result;
			Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
			Result.Implementation =
				EOpenMobileHapticControlImplementation::Emulated;
			return Result;
		}
		return OpenMobileHapticsAndroidBackendPrivate::
			MakeEmulatedControlResult(
				7,
				TEXT("Android could not stop the portable waveform.")
			);
	}
	if (!Bridge.CancelScheduled(Token.RequestId))
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
		Result.Error = FOpenMobileHapticError::FromCommon(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android scheduled request is no longer pending."),
			EOpenMobileHapticFailureStage::Playback
		);
		return Result;
	}
	FOpenMobileHapticControlResult Result;
	Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
	return Result;
}

FOpenMobileHapticControlResult FOpenMobileHapticsAndroidBackend::PausePlayback(
	const FOpenMobileHapticsBackendRequestToken& Token,
	const FOpenMobileHapticsBackendControlCommand& Command
)
{
	FOpenMobileHapticsAndroidControlledPlayback Controlled;
	if (!PlaybackControlStore.Find(Token.RequestId, Controlled))
	{
		return OpenMobileHapticsAndroidBackendPrivate::
			MakeEmulatedControlResult(
				7,
				TEXT("Android could not pause the portable waveform.")
			);
	}
	const int32 NativeResult = Bridge.PauseControlledWaveform(
		Token.RequestId,
		Command.Revision
	);
	if (NativeResult == 7)
	{
		PlaybackControlStore.Remove(Token.RequestId);
	}
	return OpenMobileHapticsAndroidBackendPrivate::MakeEmulatedControlResult(
		NativeResult,
		TEXT("Android could not pause the portable waveform.")
	);
}

FOpenMobileHapticControlResult FOpenMobileHapticsAndroidBackend::ResumePlayback(
	const FOpenMobileHapticsBackendRequestToken& Token,
	const FOpenMobileHapticsBackendControlCommand& Command
)
{
	using namespace OpenMobileHapticsAndroidBackendPrivate;
	FOpenMobileHapticsAndroidControlledPlayback Controlled;
	if (!PlaybackControlStore.Find(Token.RequestId, Controlled))
	{
		return MakeEmulatedControlResult(
			7,
			TEXT("Android could not resume the portable waveform.")
		);
	}
	const FOpenMobileHapticsAndroidPlaybackControlResolution Waveform =
		ResolveControlledWaveform(Controlled, Command);
	if (!Waveform.IsSuccess())
	{
		FOpenMobileHapticControlResult Result =
			FOpenMobileHapticControlResult::MakeRejected(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("Android could not compile the remaining portable waveform.")
			);
		Result.Implementation =
			EOpenMobileHapticControlImplementation::Emulated;
		return Result;
	}
	const int32 NativeResult = Bridge.ResumeControlledWaveform(
		Token.RequestId,
		Command.Revision,
		Waveform.TimingsMilliseconds,
		Waveform.Amplitudes,
		Waveform.RepeatIndex,
		Controlled.Purpose,
		Waveform.CompletionDurationMilliseconds
	);
	if (NativeResult == 7)
	{
		PlaybackControlStore.Remove(Token.RequestId);
	}
	return MakeEmulatedControlResult(
		NativeResult,
		TEXT("Android could not resume the portable waveform.")
	);
}

FOpenMobileHapticControlResult FOpenMobileHapticsAndroidBackend::SeekPlayback(
	const FOpenMobileHapticsBackendRequestToken& Token,
	const FOpenMobileHapticsBackendControlCommand& Command
)
{
	using namespace OpenMobileHapticsAndroidBackendPrivate;
	FOpenMobileHapticsAndroidControlledPlayback Controlled;
	if (!PlaybackControlStore.Find(Token.RequestId, Controlled))
	{
		return MakeEmulatedControlResult(
			7,
			TEXT("Android could not seek the portable waveform.")
		);
	}
	const FOpenMobileHapticsAndroidPlaybackControlResolution Waveform =
		ResolveControlledWaveform(Controlled, Command);
	if (!Waveform.IsSuccess())
	{
		FOpenMobileHapticControlResult Result =
			FOpenMobileHapticControlResult::MakeRejected(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("Android could not compile the requested waveform position.")
			);
		Result.Implementation =
			EOpenMobileHapticControlImplementation::Emulated;
		return Result;
	}
	const int32 NativeResult = Bridge.SeekControlledWaveform(
		Token.RequestId,
		Command.Revision,
		Waveform.TimingsMilliseconds,
		Waveform.Amplitudes,
		Waveform.RepeatIndex,
		Controlled.Purpose,
		Waveform.CompletionDurationMilliseconds
	);
	if (NativeResult == 7)
	{
		PlaybackControlStore.Remove(Token.RequestId);
	}
	return MakeEmulatedControlResult(
		NativeResult,
		TEXT("Android could not seek the portable waveform.")
	);
}

FOpenMobileHapticControlResult FOpenMobileHapticsAndroidBackend::StopAll()
{
	if (!IsCustomPlaybackConfigured())
	{
		return FOpenMobileHapticControlResult::MakeRejected(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("Custom Android vibration was not included in this build.")
		);
	}
	if (!Bridge.StopAll())
	{
		return FOpenMobileHapticControlResult::MakeRejected(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not stop application vibration.")
		);
	}
	PlaybackControlStore.Reset();
	FOpenMobileHapticControlResult Result;
	Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
	return Result;
}

FOpenMobileHapticsBackendSubmission
FOpenMobileHapticsAndroidBackend::SubmitOneShot(
	const FOpenMobileHapticOneShotRequest& Request,
	const FOpenMobileHapticsOneShotResolution& Resolution,
	const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
	const FOpenMobileHapticsBackendRequestToken& Token,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	if (Resolution.Path != EOpenMobileHapticsOneShotPath::SystemSemantic
		&& !IsCustomPlaybackConfigured())
	{
		return OpenMobileHapticsAndroidBackendPrivate::
			MakeNotConfiguredSubmission();
	}
	FOpenMobileHapticsBackendSubmission Submission;
	const int32 Purpose = Request.Options.Category == TEXT("Alerts")
		? 2
		: Request.Options.Category == TEXT("Gameplay")
			? 1
			: 0;
	const int64 DurationMillis = static_cast<int64>(FMath::Max(
		1.0,
		FMath::RoundToDouble(Request.DurationSeconds * 1000.0)
	));
	EOpenMobileHapticsOneShotPath SubmittedPath = Resolution.Path;
	FOpenMobileHapticsAndroidScheduledPlayback Scheduled =
		OpenMobileHapticsAndroidBackendPrivate::MakeScheduledPlayback(
			Parameters,
			TEXT("OneShot"),
			Request.Options,
			FOpenMobileHapticsOneShotPolicy::PathName(SubmittedPath),
			Callback
		);
	int32 NativeResult = Bridge.PlayOneShot(
		Token,
		DurationMillis,
		Request.Intensity,
		SubmittedPath,
		Purpose,
		Scheduled
	);
	bool bUsedFallback = false;
	bool bSelectedNoEffect = false;
	if (Resolution.Path == EOpenMobileHapticsOneShotPath::PredefinedEffect
		&& NativeResult == 4)
	{
		const FOpenMobileHapticCapabilities Capabilities = GetCapabilities();
		const bool bAllowBasic = Request.Options.FallbackPolicy
			!= EOpenMobileHapticFallbackPolicy::NoBasicVibration
			&& Request.Options.FallbackPolicy
				!= EOpenMobileHapticFallbackPolicy::ExactOnly;
		if (bAllowBasic
			&& Capabilities.BasicVibration
				== EOpenMobileHapticSupportState::Supported)
		{
			SubmittedPath = EOpenMobileHapticsOneShotPath::BasicVibration;
			Scheduled.ResolvedPath =
				FOpenMobileHapticsOneShotPolicy::PathName(SubmittedPath);
			NativeResult = Bridge.PlayOneShot(
				Token,
				DurationMillis,
				Request.Intensity,
				SubmittedPath,
				Purpose,
				MoveTemp(Scheduled)
			);
			bUsedFallback = NativeResult != 4;
		}
		if (NativeResult == 4 && Resolution.bSuppressWhenUnavailable)
		{
			NativeResult = 2;
			bSelectedNoEffect = true;
		}
	}
	Submission.Result.ResolvedPath =
		bSelectedNoEffect
			? FName(TEXT("NoEffect"))
			: FOpenMobileHapticsOneShotPolicy::PathName(SubmittedPath);
	if (SubmittedPath == EOpenMobileHapticsOneShotPath::BasicVibration
		|| NativeResult == 3
		|| NativeResult == 5)
	{
		Submission.Result.Duration.bNativeDurationKnown = true;
		Submission.Result.Duration.NativeSeconds =
			static_cast<double>(DurationMillis) / 1000.0;
		Submission.Result.Duration.bNativeClamped = !FMath::IsNearlyEqual(
			Submission.Result.Duration.NativeSeconds,
			static_cast<double>(Request.DurationSeconds),
			UE_DOUBLE_SMALL_NUMBER
		);
		const EOpenMobileHapticSupportState AmplitudeControl =
			GetCapabilities().AmplitudeControl;
		if (NativeResult == 5)
		{
			Submission.Result.Intensity.bNativeIntensityKnown = true;
			Submission.Result.Intensity.Native = 1.0f;
			Submission.Result.Intensity.bNativeClamped = true;
		}
		else if (AmplitudeControl
			== EOpenMobileHapticSupportState::Supported)
		{
			const int32 NativeAmplitude = FMath::Clamp(
				FMath::RoundToInt(Request.Intensity * 255.0f),
				1,
				255
			);
			Submission.Result.Intensity.bNativeIntensityKnown = true;
			Submission.Result.Intensity.Native =
				static_cast<float>(NativeAmplitude) / 255.0f;
			Submission.Result.Intensity.bNativeClamped = !FMath::IsNearlyEqual(
				Submission.Result.Intensity.Native,
				Request.Intensity
			);
		}
		else if (Request.Intensity == 1.0f)
		{
			Submission.Result.Intensity.bNativeIntensityKnown = true;
			Submission.Result.Intensity.Native = 1.0f;
		}
	}
	switch (NativeResult)
	{
	case 1:
		Submission.Result.Outcome = bUsedFallback
			? EOpenMobileHapticPlaybackOutcome::Fallback
			: EOpenMobileHapticPlaybackOutcome::Accepted;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
		break;
	case 2:
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Suppressed;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
		break;
	case 3:
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
		Submission.Result.ResolvedPath = TEXT("BasicVibration");
		break;
	case 4:
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Android device has no available one-shot vibration path.")
		);
		break;
	case 5:
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
		Submission.Result.ResolvedPath = TEXT("BasicVibrationDefaultAmplitude");
		break;
	default:
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not submit one-shot Haptics feedback.")
		);
		break;
	}
	Submission.bExpectsCallbacks = NativeResult == 6;
	Submission.bCreatesControllablePlayback = Submission.bExpectsCallbacks;
	OpenMobileHapticsAndroidBackendPrivate::ApplyBestEffortTiming(
		Submission,
		Parameters,
		Request.Options
	);
	return Submission;
}

FOpenMobileHapticsBackendSubmission
FOpenMobileHapticsAndroidBackend::SubmitNamedPattern(
	const FOpenMobileHapticNamedPatternRequest& Request,
	const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
	const FOpenMobileHapticsBackendRequestToken& Token,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	using namespace OpenMobileHapticsAndroidBackendPrivate;
	const bool bCustomPlaybackConfigured = IsCustomPlaybackConfigured();
	const FOpenMobileHapticCapabilities Capabilities = GetCapabilities();
	const int32 Purpose = PurposeFor(Request.Options.Category);
	const int32 AndroidAPI = FAndroidMisc::GetAndroidBuildVersion();
	const UOpenMobileHapticPatternAsset* PortablePattern =
		Cast<UOpenMobileHapticPatternAsset>(
			Request.PatternAsset.ResolveObject()
		);
	TArray<FName> RichAttempts;
	const UOpenMobileHapticAndroidPatternAsset* Asset =
		Cast<UOpenMobileHapticAndroidPatternAsset>(
			Request.PlatformOverrideAsset.ResolveObject()
		);
	if (!bCustomPlaybackConfigured && !PortablePattern)
	{
		return MakeNotConfiguredSubmission();
	}
	if (PortablePattern)
	{
		const FOpenMobileHapticsPlatformOverrideResolution Override =
			FOpenMobileHapticsPlatformOverridePolicy::Resolve(
				*PortablePattern,
				EOpenMobileHapticOverridePlatform::Android,
				AndroidAPI,
				Capabilities,
				Request.Options.FallbackPolicy
			);
		const FOpenMobileHapticsFallbackResolution Ladder =
			FOpenMobileHapticsFallbackPolicy::Resolve(
				*PortablePattern,
				Override,
				Capabilities,
				Request.Options.FallbackPolicy
			);
		RichAttempts =
			FOpenMobileHapticsFallbackPolicy::MakeDiagnosticTrace(Ladder);
		if (Ladder.Path != EOpenMobileHapticsFallbackPath::ExactOverride)
		{
			return SubmitPortableAndFallback(
				Bridge,
				PlaybackControlStore,
				Request,
				Token,
				*PortablePattern,
				Capabilities,
				Parameters,
				Purpose,
				MoveTemp(Callback),
				MoveTemp(RichAttempts)
			);
		}
		Asset = Cast<UOpenMobileHapticAndroidPatternAsset>(
			Override.OverrideAsset.ResolveObject()
		);
	}
	if (!Asset)
	{
		if (PortablePattern)
		{
			RichAttempts.Add(TEXT("ExactOverride:MissingLoadedAsset"));
			return SubmitPortableAndFallback(
				Bridge,
				PlaybackControlStore,
				Request,
				Token,
				*PortablePattern,
				Capabilities,
				Parameters,
				Purpose,
				MoveTemp(Callback),
				MoveTemp(RichAttempts)
			);
		}
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The named pattern has no loaded Android override.")
		);
		return Submission;
	}

	if (Asset->Format == EOpenMobileHapticAndroidPatternFormat::Primitives)
	{
		const FOpenMobileHapticsPrimitiveCompositionResolution Resolution =
			FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
				*Asset,
				Capabilities,
				AndroidAPI,
				Request.Intensity,
				Request.Options.FallbackPolicy
			);
		if (Resolution.Outcome
			!= EOpenMobileHapticsPrimitiveCompositionOutcome::Ready)
		{
			if (PortablePattern)
			{
				RichAttempts.Add(Attempt(
					TEXT("ExactOverride"),
					Resolution.Reason
				));
				return SubmitPortableAndFallback(
					Bridge,
					PlaybackControlStore,
					Request,
					Token,
					*PortablePattern,
					Capabilities,
					Parameters,
					Purpose,
					MoveTemp(Callback),
					MoveTemp(RichAttempts)
				);
			}
			FOpenMobileHapticsBackendSubmission Submission;
			if (Resolution.Outcome
				== EOpenMobileHapticsPrimitiveCompositionOutcome::FallbackRequired
				&& Request.Options.FallbackPolicy
					== EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
			{
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Suppressed;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Completed;
				Submission.Result.ResolvedPath = TEXT("NoEffect");
				Submission.Result.FallbackAttempts.Add(Resolution.Reason);
				return Submission;
			}
			Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
				EOpenMobileErrorCode::NotSupported,
				TEXT("The Android primitive composition is unavailable on this device.")
			);
			Submission.Result.FallbackAttempts.Add(Resolution.Reason);
			return Submission;
		}
		const FName ResolvedPath(TEXT("AndroidPrimitiveComposition"));
		const int32 NativeResult = Bridge.PlayPrimitives(
				Token,
				Resolution.Primitives,
				Resolution.Scales,
				Resolution.DelaysMilliseconds,
				Purpose,
				MakeScheduledPlayback(
					Parameters,
					Request,
					ResolvedPath,
					Callback
				)
			);
		if (NativeResult == 4 && PortablePattern)
		{
			RichAttempts.Add(TEXT("ExactOverride:NativeSupportChanged"));
			return SubmitPortableAndFallback(
				Bridge,
				PlaybackControlStore,
				Request,
				Token,
				*PortablePattern,
				Capabilities,
				Parameters,
				Purpose,
				MoveTemp(Callback),
				MoveTemp(RichAttempts)
			);
		}
		FOpenMobileHapticsBackendSubmission Submission = MakeNativeSubmission(
			NativeResult,
			ResolvedPath,
			false,
			TEXT("Android rejected an unsupported primitive composition."),
			TEXT("Android could not submit the primitive composition.")
		);
		ApplyBestEffortTiming(Submission, Parameters, Request);
		return Submission;
	}
	if (Asset->Format == EOpenMobileHapticAndroidPatternFormat::Waveform)
	{
		const FOpenMobileHapticsAndroidWaveformResolution Waveform =
			FOpenMobileHapticsAndroidWaveformPolicy::ResolveOverride(
				*Asset,
				Capabilities,
				AndroidAPI,
				Request.Intensity,
				Request.Options.FallbackPolicy
			);
		if (Waveform.Outcome
			== EOpenMobileHapticsAndroidWaveformOutcome::Ready)
		{
			const FName ResolvedPath = Waveform.bUsesDefaultAmplitude
				? FName(TEXT("AndroidWaveformDefaultAmplitude"))
				: FName(TEXT("AndroidWaveform"));
			const int32 NativeResult = Bridge.PlayWaveform(
				Token,
				0,
				Waveform.TimingsMilliseconds,
				Waveform.Amplitudes,
				Waveform.RepeatIndex,
				Purpose,
				MakeScheduledPlayback(
					Parameters,
					Request,
					ResolvedPath,
					Callback
				)
			);
			if (NativeResult != 4 || !PortablePattern)
			{
				FOpenMobileHapticsBackendSubmission Submission =
					MakeNativeSubmission(
						NativeResult,
						ResolvedPath,
						Waveform.bUsesDefaultAmplitude || NativeResult == 5,
						TEXT("Android rejected the waveform override."),
						TEXT("Android could not submit the waveform override.")
					);
				if (Waveform.bUsesDefaultAmplitude || NativeResult == 5)
				{
					Submission.Result.Intensity.bNativeClamped = true;
					Submission.Result.FallbackAttempts.Add(
						TEXT("AmplitudeControl:Default")
					);
				}
				ApplyBestEffortTiming(Submission, Parameters, Request);
				return Submission;
			}
			RichAttempts.Add(TEXT("ExactOverride:NativeSupportChanged"));
		}
		else
		{
			RichAttempts.Add(Attempt(
				TEXT("ExactOverride"),
				Waveform.Reason
			));
			if (!PortablePattern)
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result =
					FOpenMobileHapticPlaybackResult::MakeRejected(
						Waveform.Outcome
							== EOpenMobileHapticsAndroidWaveformOutcome::Rejected
								? EOpenMobileErrorCode::InvalidArgument
								: EOpenMobileErrorCode::NotSupported,
						TEXT("The Android waveform override is unavailable.")
					);
				AppendAttempts(Submission, RichAttempts);
				return Submission;
			}
		}
		return SubmitPortableAndFallback(
			Bridge,
			PlaybackControlStore,
			Request,
			Token,
			*PortablePattern,
			Capabilities,
			Parameters,
			Purpose,
			MoveTemp(Callback),
			MoveTemp(RichAttempts)
		);
	}

	if (Asset->Format != EOpenMobileHapticAndroidPatternFormat::BasicEnvelope
		&& Asset->Format
			!= EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Android pattern format is not connected to playback yet.")
		);
		return Submission;
	}

	const FOpenMobileHapticsEnvelopeResolution Envelope =
		FOpenMobileHapticsEnvelopePolicy::Resolve(
			*Asset,
			Capabilities,
			AndroidAPI,
			Request.Intensity,
			Request.Options.FallbackPolicy
		);
	FName EnvelopeFailureReason = Envelope.Reason;
	if (Envelope.Outcome == EOpenMobileHapticsEnvelopeOutcome::Ready)
	{
		const FName ResolvedPath = Envelope.Format
			== EOpenMobileHapticAndroidPatternFormat::BasicEnvelope
				? FName(TEXT("AndroidBasicEnvelope"))
				: FName(TEXT("AndroidWaveformEnvelope"));
		const int32 NativeResult = Bridge.PlayEnvelope(
			Token,
			Envelope.Format,
			Envelope.Amplitudes,
			Envelope.ControlValues,
			Envelope.DurationsMilliseconds,
			Purpose,
			MakeScheduledPlayback(
				Parameters,
				Request,
				ResolvedPath,
				Callback
			)
		);
		if (NativeResult != 4)
		{
			FOpenMobileHapticsBackendSubmission Submission = MakeNativeSubmission(
				NativeResult,
				ResolvedPath,
				false,
				TEXT("Android rejected an unsupported envelope."),
				TEXT("Android could not submit the envelope.")
			);
			ApplyBestEffortTiming(Submission, Parameters, Request);
			return Submission;
		}
		EnvelopeFailureReason = TEXT("NativeSupportChanged");
	}

	if (Envelope.Outcome == EOpenMobileHapticsEnvelopeOutcome::Rejected)
	{
		if (PortablePattern && Envelope.Reason != TEXT("InvalidIntensity"))
		{
			RichAttempts.Add(Attempt(
				TEXT("ExactOverride"),
				EnvelopeFailureReason
			));
			return SubmitPortableAndFallback(
				Bridge,
				PlaybackControlStore,
				Request,
				Token,
				*PortablePattern,
				Capabilities,
				Parameters,
				Purpose,
				MoveTemp(Callback),
				MoveTemp(RichAttempts)
			);
		}
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			Envelope.Reason == TEXT("InvalidIntensity")
				|| Envelope.Reason == TEXT("InvalidFormat")
				|| Envelope.Reason == TEXT("InvalidPattern")
				|| Envelope.Reason == TEXT("ConfiguredPointCount")
				|| Envelope.Reason == TEXT("Time")
					? EOpenMobileErrorCode::InvalidArgument
					: EOpenMobileErrorCode::NotSupported,
			TEXT("The Android envelope was rejected before native submission.")
		);
		Submission.Result.FallbackAttempts.Add(EnvelopeFailureReason);
		return Submission;
	}
	if (PortablePattern)
	{
		RichAttempts.Add(Attempt(
			TEXT("ExactOverride"),
			EnvelopeFailureReason
		));
		return SubmitPortableAndFallback(
			Bridge,
			PlaybackControlStore,
			Request,
			Token,
			*PortablePattern,
			Capabilities,
			Parameters,
			Purpose,
			MoveTemp(Callback),
			MoveTemp(RichAttempts)
		);
	}

	if (Request.Options.FallbackPolicy
		== EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Suppressed;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
		Submission.Result.ResolvedPath = TEXT("NoEffect");
		Submission.Result.FallbackAttempts.Add(EnvelopeFailureReason);
		return Submission;
	}

	FOpenMobileHapticsBackendSubmission Submission;
	Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
		EOpenMobileErrorCode::NotSupported,
		TEXT("The Android envelope and its declared fallbacks are unavailable.")
	);
	Submission.Result.FallbackAttempts.Add(EnvelopeFailureReason);
	return Submission;
}

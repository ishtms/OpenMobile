#include "OpenMobileHapticsAndroidBackend.h"

#include "Android/AndroidPlatformMisc.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsAndroidConfigurationPolicy.h"
#include "OpenMobileHapticsAndroidFallbackPolicy.h"
#include "OpenMobileHapticsAndroidWaveformPolicy.h"
#include "OpenMobileHapticsEnvelopePolicy.h"
#include "OpenMobileHapticsFallbackPolicy.h"
#include "OpenMobileHapticsPlatformOverridePolicy.h"
#include "OpenMobileHapticsPrimitiveCompositionPolicy.h"
#include "OpenMobileHapticsSemanticPolicy.h"
#include "OpenMobileHapticsSettings.h"

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
			Submission.Result.Outcome = bFallback
				|| NativeResult == 3 || NativeResult == 5
				? EOpenMobileHapticPlaybackOutcome::Fallback
				: EOpenMobileHapticPlaybackOutcome::Accepted;
			Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
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

	FOpenMobileHapticsBackendSubmission SubmitPortableAndFallback(
		FOpenMobileHapticsAndroidBridge& Bridge,
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendRequestToken& Token,
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 Purpose,
		FOpenMobileHapticsBackendEventCallback Callback,
		TArray<FName> Attempts
	)
	{
		const FOpenMobileHapticsAndroidWaveformResolution Portable =
			FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
				Pattern,
				Capabilities,
				Request.Intensity,
				Request.Options.FallbackPolicy
			);
		if (Portable.Outcome
			== EOpenMobileHapticsAndroidWaveformOutcome::Ready)
		{
			const int32 NativeResult = Bridge.PlayWaveform(
				Token,
				Portable.TimingsMilliseconds,
				Portable.Amplitudes,
				Portable.RepeatIndex,
				Purpose
			);
			if (NativeResult != 4)
			{
				FOpenMobileHapticsBackendSubmission Submission =
					MakeNativeSubmission(
						NativeResult,
						Portable.bUsesDefaultAmplitude
							? FName(TEXT("AndroidPortableWaveformDefaultAmplitude"))
							: FName(TEXT("AndroidPortableWaveform")),
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
				const int32 NativeResult = Bridge.PlayPrimitives(
					Token,
					{Fallback.Primitive},
					{Request.Intensity},
					{0},
					Purpose
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
						TEXT("AndroidPrimitiveFallback"),
						true,
						TEXT("Android rejected the declared primitive fallback."),
						TEXT("Android could not submit the primitive fallback.")
					);
				AppendAttempts(Submission, Attempts);
				return Submission;
			}
			if (Fallback.Outcome
				== EOpenMobileHapticsAndroidFallbackOutcome::Predefined)
			{
				const int32 NativeResult = Bridge.PlayPredefined(
					Token,
					static_cast<int32>(Fallback.PredefinedEffect),
					Purpose
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
						TEXT("AndroidPredefinedFallback"),
						true,
						TEXT("Android rejected the declared predefined fallback."),
						TEXT("Android could not submit the predefined fallback.")
					);
				AppendAttempts(Submission, Attempts);
				return Submission;
			}
			if (Fallback.Outcome
				== EOpenMobileHapticsAndroidFallbackOutcome::Semantic)
			{
				const FOpenMobileHapticsSemanticDescriptor Descriptor =
					FOpenMobileHapticsSemanticPolicy::Describe(
						Fallback.SemanticEffect
					);
				const FOpenMobileHapticsAndroidBridgeSubmission Native =
					Bridge.PlaySemantic(
						Token,
						Descriptor.Behavior,
						Request.Intensity,
						EOpenMobileHapticsSemanticPath::SystemSemantic,
						Purpose,
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
					Purpose
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
	Support.bStopAll = IsCustomPlaybackConfigured();
	return Support;
}

FOpenMobileHapticCapabilities
FOpenMobileHapticsAndroidBackend::ProbeHardwareCapabilities() const
{
	using namespace OpenMobileHapticsAndroidBackendPrivate;
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.BackendName = GetBackendName();
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
	Capabilities.Scheduling = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Pause = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Resume = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Seek = EOpenMobileHapticSupportState::Unsupported;
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

FOpenMobileHapticsBackendSubmission
FOpenMobileHapticsAndroidBackend::SubmitSemantic(
	const FOpenMobileHapticSemanticRequest& Request,
	const FOpenMobileHapticsSemanticResolution& Resolution,
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
	FOpenMobileHapticsBackendEventCallback RetryCallback = Callback;
	FOpenMobileHapticsAndroidBridgeSubmission BridgeSubmission =
		Bridge.PlaySemantic(
		Token,
		Descriptor.Behavior,
		Request.Intensity,
		SubmittedPath,
		Purpose,
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
	return Submission;
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
	FOpenMobileHapticControlResult Result;
	Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
	return Result;
}

FOpenMobileHapticsBackendSubmission
FOpenMobileHapticsAndroidBackend::SubmitOneShot(
	const FOpenMobileHapticOneShotRequest& Request,
	const FOpenMobileHapticsOneShotResolution& Resolution,
	const FOpenMobileHapticsBackendRequestToken& Token,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	static_cast<void>(Callback);
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
	int32 NativeResult = Bridge.PlayOneShot(
		Token,
		DurationMillis,
		Request.Intensity,
		SubmittedPath,
		Purpose
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
			NativeResult = Bridge.PlayOneShot(
				Token,
				DurationMillis,
				Request.Intensity,
				SubmittedPath,
				Purpose
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
	static_cast<void>(Parameters);
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
				Request,
				Token,
				*PortablePattern,
				Capabilities,
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
				Request,
				Token,
				*PortablePattern,
				Capabilities,
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
					Request,
					Token,
					*PortablePattern,
					Capabilities,
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
		const int32 NativeResult = Bridge.PlayPrimitives(
				Token,
				Resolution.Primitives,
				Resolution.Scales,
				Resolution.DelaysMilliseconds,
				Purpose
			);
		if (NativeResult == 4 && PortablePattern)
		{
			RichAttempts.Add(TEXT("ExactOverride:NativeSupportChanged"));
			return SubmitPortableAndFallback(
				Bridge,
				Request,
				Token,
				*PortablePattern,
				Capabilities,
				Purpose,
				MoveTemp(Callback),
				MoveTemp(RichAttempts)
			);
		}
		return MakeNativeSubmission(
			NativeResult,
			TEXT("AndroidPrimitiveComposition"),
			false,
			TEXT("Android rejected an unsupported primitive composition."),
			TEXT("Android could not submit the primitive composition.")
		);
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
			const int32 NativeResult = Bridge.PlayWaveform(
				Token,
				Waveform.TimingsMilliseconds,
				Waveform.Amplitudes,
				Waveform.RepeatIndex,
				Purpose
			);
			if (NativeResult != 4 || !PortablePattern)
			{
				FOpenMobileHapticsBackendSubmission Submission =
					MakeNativeSubmission(
						NativeResult,
						Waveform.bUsesDefaultAmplitude
							? FName(TEXT("AndroidWaveformDefaultAmplitude"))
							: FName(TEXT("AndroidWaveform")),
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
			Request,
			Token,
			*PortablePattern,
			Capabilities,
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
		const int32 NativeResult = Bridge.PlayEnvelope(
			Token,
			Envelope.Format,
			Envelope.Amplitudes,
			Envelope.ControlValues,
			Envelope.DurationsMilliseconds,
			Purpose
		);
		if (NativeResult != 4)
		{
			return MakeNativeSubmission(
				NativeResult,
				Envelope.Format
					== EOpenMobileHapticAndroidPatternFormat::BasicEnvelope
						? FName(TEXT("AndroidBasicEnvelope"))
						: FName(TEXT("AndroidWaveformEnvelope")),
				false,
				TEXT("Android rejected an unsupported envelope."),
				TEXT("Android could not submit the envelope.")
			);
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
				Request,
				Token,
				*PortablePattern,
				Capabilities,
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
			Request,
			Token,
			*PortablePattern,
			Capabilities,
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

#include "OpenMobileHapticsAndroidBackend.h"

#include "Android/AndroidPlatformMisc.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsAndroidFallbackPolicy.h"
#include "OpenMobileHapticsEnvelopePolicy.h"
#include "OpenMobileHapticsPrimitiveCompositionPolicy.h"

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
			Submission.Result.Outcome = bFallback
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
	const FOpenMobileHapticsSemanticDescriptor Descriptor =
		FOpenMobileHapticsSemanticPolicy::Describe(Request.Effect);
	const int32 Purpose = Request.Options.Category == TEXT("Alerts")
		? 2
		: Request.Options.Category == TEXT("Gameplay")
			? 1
			: 0;
	const FName ResolvedPath =
		FOpenMobileHapticsSemanticPolicy::PathName(Resolution.Path);
	const FOpenMobileHapticsAndroidBridgeSubmission BridgeSubmission =
		Bridge.PlaySemantic(
		Token,
		Descriptor.Behavior,
		Request.Intensity,
		Resolution.Path,
		Purpose,
		Descriptor.Name,
		Request.Options.Channel,
		ResolvedPath,
		MoveTemp(Callback)
	);
	Submission.Result.ResolvedPath = ResolvedPath;
	Submission.bExpectsCallbacks = BridgeSubmission.bExpectsCallback;
	switch (BridgeSubmission.Result)
	{
	case 1:
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Accepted;
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
	const int32 NativeResult = Bridge.PlayOneShot(
		Token,
		DurationMillis,
		Request.Intensity,
		Resolution.Path,
		Purpose
	);
	Submission.Result.ResolvedPath =
		FOpenMobileHapticsOneShotPolicy::PathName(Resolution.Path);
	if (Resolution.Path == EOpenMobileHapticsOneShotPath::BasicVibration
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
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Accepted;
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
	const FOpenMobileHapticsBackendRequestToken& Token,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	using namespace OpenMobileHapticsAndroidBackendPrivate;
	static_cast<void>(Callback);
	const UOpenMobileHapticAndroidPatternAsset* Asset =
		Cast<UOpenMobileHapticAndroidPatternAsset>(
			Request.PlatformOverrideAsset.ResolveObject()
		);
	if (!Asset)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The named pattern has no loaded Android override.")
		);
		return Submission;
	}

	const FOpenMobileHapticCapabilities Capabilities = GetCapabilities();
	const int32 Purpose = PurposeFor(Request.Options.Category);
	if (Asset->Format == EOpenMobileHapticAndroidPatternFormat::Primitives)
	{
		const FOpenMobileHapticsPrimitiveCompositionResolution Resolution =
			FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
				*Asset,
				Capabilities,
				FAndroidMisc::GetAndroidBuildVersion(),
				Request.Intensity,
				Request.Options.FallbackPolicy
			);
		if (Resolution.Outcome
			!= EOpenMobileHapticsPrimitiveCompositionOutcome::Ready)
		{
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
		return MakeNativeSubmission(
			Bridge.PlayPrimitives(
				Token,
				Resolution.Primitives,
				Resolution.Scales,
				Resolution.DelaysMilliseconds,
				Purpose
			),
			TEXT("AndroidPrimitiveComposition"),
			false,
			TEXT("Android rejected an unsupported primitive composition."),
			TEXT("Android could not submit the primitive composition.")
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
			FAndroidMisc::GetAndroidBuildVersion(),
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

	const UOpenMobileHapticPatternAsset* Pattern =
		Cast<UOpenMobileHapticPatternAsset>(Request.PatternAsset.ResolveObject());
	if (Pattern)
	{
		const FOpenMobileHapticsAndroidFallbackResolution Fallback =
			FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
				*Pattern,
				Capabilities,
				Request.Options.FallbackPolicy
			);
		if (Fallback.Outcome
			== EOpenMobileHapticsAndroidFallbackOutcome::Primitive)
		{
			const TArray<EOpenMobileHapticAndroidPrimitive> Primitives = {
				Fallback.Primitive
			};
			const TArray<float> Scales = {Request.Intensity};
			const TArray<int32> Delays = {0};
			FOpenMobileHapticsBackendSubmission Submission =
				MakeNativeSubmission(
					Bridge.PlayPrimitives(
						Token,
						Primitives,
						Scales,
						Delays,
						Purpose
					),
					TEXT("AndroidPrimitiveFallback"),
					true,
					TEXT("Android rejected the declared primitive fallback."),
					TEXT("Android could not submit the primitive fallback.")
				);
			Submission.Result.FallbackAttempts.Add(EnvelopeFailureReason);
			Submission.Result.FallbackAttempts.Append(Fallback.Attempts);
			if (Submission.Result.Error.CommonCode
					== EOpenMobileErrorCode::NotSupported
				&& (Request.Options.FallbackPolicy
						== EOpenMobileHapticFallbackPolicy::NoEffectAllowed
					|| Pattern->FallbackPolicy
						== EOpenMobileHapticFallbackPolicy::NoEffectAllowed))
			{
				Submission.Result = {};
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Suppressed;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Completed;
				Submission.Result.ResolvedPath = TEXT("NoEffect");
				Submission.Result.FallbackAttempts.Add(
					EnvelopeFailureReason
				);
				Submission.Result.FallbackAttempts.Append(Fallback.Attempts);
				Submission.Result.FallbackAttempts.Add(
					TEXT("NoEffect:Selected")
				);
			}
			return Submission;
		}
		if (Fallback.Outcome
			== EOpenMobileHapticsAndroidFallbackOutcome::NoEffect)
		{
			FOpenMobileHapticsBackendSubmission Submission;
			Submission.Result.Outcome =
				EOpenMobileHapticPlaybackOutcome::Suppressed;
			Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
			Submission.Result.ResolvedPath = TEXT("NoEffect");
			Submission.Result.FallbackAttempts.Add(EnvelopeFailureReason);
			Submission.Result.FallbackAttempts.Append(Fallback.Attempts);
			return Submission;
		}
	}
	else if (Request.Options.FallbackPolicy
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

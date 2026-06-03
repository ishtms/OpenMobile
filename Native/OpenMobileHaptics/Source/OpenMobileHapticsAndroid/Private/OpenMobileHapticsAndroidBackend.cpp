#include "OpenMobileHapticsAndroidBackend.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"

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

	struct FHardwareProbe
	{
		int64 Flags = ProbeUnavailable;
		uint64 PresetSupport = 0;
		uint64 PrimitiveSupport = 0;
		int64 MaximumControlPointCount = -1;
		int64 MaximumDurationMillis = -1;
		int64 MinimumTimingMillis = -1;
	};

	FHardwareProbe QueryHardware()
	{
		FHardwareProbe Probe;
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Env || !Activity)
		{
			return Probe;
		}

		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID Method = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"AndroidThunkJava_OpenMobileHapticsQueryCapabilities",
				"()[J"
			)
			: nullptr;
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return Probe;
		}
		if (!Method)
		{
			return Probe;
		}

		FScopedJavaObject<jlongArray> Values(static_cast<jlongArray>(
			Env->CallObjectMethod(Activity, Method)
		));
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return Probe;
		}
		if (!Values)
		{
			return Probe;
		}

		const jsize Count = Env->GetArrayLength(*Values);
		if (Env->ExceptionCheck() || Count < 1)
		{
			Env->ExceptionClear();
			return Probe;
		}
		jlong NativeValues[6] = {-1, 0, 0, -1, -1, -1};
		const jsize CopyCount = FMath::Min<jsize>(Count, 6);
		Env->GetLongArrayRegion(*Values, 0, CopyCount, NativeValues);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return Probe;
		}
		Probe.Flags = static_cast<int64>(NativeValues[0]);
		Probe.PresetSupport = static_cast<uint64>(NativeValues[1]);
		Probe.PrimitiveSupport = static_cast<uint64>(NativeValues[2]);
		Probe.MaximumControlPointCount =
			static_cast<int64>(NativeValues[3]);
		Probe.MaximumDurationMillis = static_cast<int64>(NativeValues[4]);
		Probe.MinimumTimingMillis = static_cast<int64>(NativeValues[5]);
		return Probe;
	}

	int32 PlaySemantic(
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity,
		EOpenMobileHapticsSemanticPath Path,
		int32 Purpose
	)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Env || !Activity)
		{
			return 0;
		}
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID Method = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"AndroidThunkJava_OpenMobileHapticsPlaySemantic",
				"(IFII)I"
			)
			: nullptr;
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return 0;
		}
		if (!Method)
		{
			return 0;
		}
		const jint Result = Env->CallIntMethod(
			Activity,
			Method,
			static_cast<jint>(Behavior),
			static_cast<jfloat>(Intensity),
			static_cast<jint>(Path),
			static_cast<jint>(Purpose)
		);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return 0;
		}
		return static_cast<int32>(Result);
	}

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
		const FHardwareProbe& Probe
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
}

FOpenMobileHapticCapabilities
FOpenMobileHapticsAndroidBackend::ProbeHardwareCapabilities() const
{
	using namespace OpenMobileHapticsAndroidBackendPrivate;
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.BackendName = GetBackendName();
	const FHardwareProbe Probe = QueryHardware();
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
	static_cast<void>(Token);
	static_cast<void>(Callback);
	FOpenMobileHapticsBackendSubmission Submission;
	const FOpenMobileHapticsSemanticDescriptor Descriptor =
		FOpenMobileHapticsSemanticPolicy::Describe(Request.Effect);
	const int32 Purpose = Request.Options.Category == TEXT("Alerts")
		? 2
		: Request.Options.Category == TEXT("Gameplay")
			? 1
			: 0;
	const int32 NativeResult = OpenMobileHapticsAndroidBackendPrivate::PlaySemantic(
		Descriptor.Behavior,
		Request.Intensity,
		Resolution.Path,
		Purpose
	);
	Submission.Result.ResolvedPath =
		FOpenMobileHapticsSemanticPolicy::PathName(Resolution.Path);
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
		break;
	case 4:
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Android device has no available vibration path.")
		);
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

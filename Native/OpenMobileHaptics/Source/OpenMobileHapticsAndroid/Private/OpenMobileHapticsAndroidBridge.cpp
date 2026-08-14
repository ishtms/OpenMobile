#include "OpenMobileHapticsAndroidBridge.h"

#include "Android/AndroidApplication.h"
#include "Async/Async.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileHapticsBackendRegistry.h"

namespace OpenMobileHapticsAndroidBridgePrivate
{
	constexpr int32 ResultAccepted = 1;
	constexpr int32 ResultSuppressed = 2;
	constexpr int32 ResultFallback = 3;
	constexpr int32 ResultDefaultAmplitude = 5;
	constexpr int32 ResultPending = 6;
	constexpr int32 ResultStale = 7;
	constexpr int32 ControlledEventStarted = 1;
	constexpr int32 ControlledEventCompleted = 2;
	constexpr int32 ControlledEventInterrupted = 3;
	constexpr int32 ControlledEventFailed = 4;

	FCriticalSection ActiveBridgeMutex;
	FOpenMobileHapticsAndroidBridge* ActiveBridge = nullptr;
}

/** Lets a delayed Java runnable verify request and lifecycle ownership immediately before touching the vibrator. */
JNI_METHOD jboolean Java_com_openmobile_haptics_OpenMobileHapticsBridgeV1_nativeCanStart(
	JNIEnv* Env,
	jclass Class,
	jlong RequestId
)
{
	static_cast<void>(Env);
	static_cast<void>(Class);
	FScopeLock Lock(
		&OpenMobileHapticsAndroidBridgePrivate::ActiveBridgeMutex
	);
	return OpenMobileHapticsAndroidBridgePrivate::ActiveBridge
		&& OpenMobileHapticsAndroidBridgePrivate::ActiveBridge->HandleCanStart(
			static_cast<uint64>(RequestId)
		)
			? JNI_TRUE
			: JNI_FALSE;
}

/** Forwards terminal scheduled-playback result to the currently registered Android bridge only. */
JNI_METHOD void Java_com_openmobile_haptics_OpenMobileHapticsBridgeV1_nativeOnBridgeResult(
	JNIEnv* Env,
	jclass Class,
	jlong RequestId,
	jint Result
)
{
	static_cast<void>(Env);
	static_cast<void>(Class);
	FScopeLock Lock(
		&OpenMobileHapticsAndroidBridgePrivate::ActiveBridgeMutex
	);
	if (OpenMobileHapticsAndroidBridgePrivate::ActiveBridge)
	{
		OpenMobileHapticsAndroidBridgePrivate::ActiveBridge->HandleBridgeResult(
			static_cast<uint64>(RequestId),
			static_cast<int32>(Result)
		);
	}
}

/** Converts Java service loss or activity replacement into backend registry interruption. */
JNI_METHOD void Java_com_openmobile_haptics_OpenMobileHapticsBridgeV1_nativeOnInterruption(
	JNIEnv* Env,
	jclass Class,
	jint Reason
)
{
	static_cast<void>(Env);
	static_cast<void>(Class);
	const EOpenMobileHapticsInterruptionReason InterruptionReason = Reason == 1
		? EOpenMobileHapticsInterruptionReason::ActivityReplaced
		: EOpenMobileHapticsInterruptionReason::NativeServiceLost;
	AsyncTask(ENamedThreads::GameThread, [InterruptionReason]()
	{
		FOpenMobileHapticsBackendRegistry::NotifyInterruption(
			TEXT("Android"),
			InterruptionReason
		);
	});
}

/** Returns revisioned controlled-waveform state with its native sequence so stale callbacks can be filtered. */
JNI_METHOD void Java_com_openmobile_haptics_OpenMobileHapticsBridgeV1_nativeOnControlledWaveformEvent(
	JNIEnv* Env,
	jclass Class,
	jlong RequestId,
	jlong ControlRevision,
	jlong EventSequence,
	jint Event
)
{
	static_cast<void>(Env);
	static_cast<void>(Class);
	FScopeLock Lock(
		&OpenMobileHapticsAndroidBridgePrivate::ActiveBridgeMutex
	);
	if (OpenMobileHapticsAndroidBridgePrivate::ActiveBridge)
	{
		OpenMobileHapticsAndroidBridgePrivate::ActiveBridge
			->HandleControlledWaveformEvent(
				static_cast<uint64>(RequestId),
				static_cast<uint64>(ControlRevision),
				static_cast<uint64>(EventSequence),
				static_cast<int32>(Event)
			);
	}
}

FOpenMobileHapticsAndroidBridge::FOpenMobileHapticsAndroidBridge()
{
	FScopeLock Lock(
		&OpenMobileHapticsAndroidBridgePrivate::ActiveBridgeMutex
	);
	OpenMobileHapticsAndroidBridgePrivate::ActiveBridge = this;
}

FOpenMobileHapticsAndroidBridge::~FOpenMobileHapticsAndroidBridge()
{
	Shutdown();
}

void FOpenMobileHapticsAndroidBridge::ClearException(JNIEnv* Env)
{
	if (Env && Env->ExceptionCheck())
	{
		Env->ExceptionClear();
	}
}

bool FOpenMobileHapticsAndroidBridge::EnsureInitialized(JNIEnv* Env)
{
	if (BridgeClass)
	{
		return true;
	}
	if (!Env)
	{
		return false;
	}

	BridgeClass = FAndroidApplication::FindJavaClassGlobalRef(
		"com/openmobile/haptics/OpenMobileHapticsBridgeV1"
	);
	if (!BridgeClass)
	{
		ClearException(Env);
		return false;
	}
	QueryCapabilitiesMethod = Env->GetStaticMethodID(
		BridgeClass,
		"queryCapabilities",
		"(Landroid/app/Activity;)[J"
	);
	PlaySemanticMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playSemantic",
		"(Landroid/app/Activity;JIFIIJ)I"
	);
	PlayOneShotMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playOneShot",
		"(Landroid/app/Activity;JJFIIJ)I"
	);
	PrepareWaveformMethod = Env->GetStaticMethodID(
		BridgeClass,
		"prepareWaveform",
		"(Landroid/app/Activity;J[J[IIJIJJ)I"
	);
	PlayWaveformMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playWaveform",
		"(Landroid/app/Activity;JJ[J[IIIJ)I"
	);
	PlayControlledWaveformMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playControlledWaveform",
		"(Landroid/app/Activity;JJ[J[IIIJJ)I"
	);
	PauseControlledWaveformMethod = Env->GetStaticMethodID(
		BridgeClass,
		"pauseControlledWaveform",
		"(Landroid/app/Activity;JJ)I"
	);
	ResumeControlledWaveformMethod = Env->GetStaticMethodID(
		BridgeClass,
		"resumeControlledWaveform",
		"(Landroid/app/Activity;JJ[J[IIIJ)I"
	);
	SeekControlledWaveformMethod = Env->GetStaticMethodID(
		BridgeClass,
		"seekControlledWaveform",
		"(Landroid/app/Activity;JJ[J[IIIJ)I"
	);
	StopControlledWaveformMethod = Env->GetStaticMethodID(
		BridgeClass,
		"stopControlledWaveform",
		"(J)Z"
	);
	PlayPredefinedMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playPredefined",
		"(Landroid/app/Activity;JIIJ)I"
	);
	PlayPrimitivesMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playPrimitives",
		"(Landroid/app/Activity;J[I[F[IIJ)I"
	);
	PlayEnvelopeMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playEnvelope",
		"(Landroid/app/Activity;JI[F[F[JIJ)I"
	);
	StopAllMethod = Env->GetStaticMethodID(
		BridgeClass,
		"stopAll",
		"(Landroid/app/Activity;)Z"
	);
	CancelScheduledMethod = Env->GetStaticMethodID(
		BridgeClass,
		"cancelScheduledRequest",
		"(J)Z"
	);
	ReleasePreparedResourcesMethod = Env->GetStaticMethodID(
		BridgeClass,
		"releasePreparedResources",
		"()V"
	);
	if (Env->ExceptionCheck()
		|| !QueryCapabilitiesMethod
		|| !PlaySemanticMethod
		|| !PlayOneShotMethod
		|| !PrepareWaveformMethod
		|| !PlayWaveformMethod
		|| !PlayControlledWaveformMethod
		|| !PauseControlledWaveformMethod
		|| !ResumeControlledWaveformMethod
		|| !SeekControlledWaveformMethod
		|| !StopControlledWaveformMethod
		|| !PlayPredefinedMethod
		|| !PlayPrimitivesMethod
		|| !PlayEnvelopeMethod
		|| !StopAllMethod
		|| !CancelScheduledMethod
		|| !ReleasePreparedResourcesMethod)
	{
		ClearException(Env);
		Env->DeleteGlobalRef(BridgeClass);
		BridgeClass = nullptr;
		QueryCapabilitiesMethod = nullptr;
		PlaySemanticMethod = nullptr;
		PlayOneShotMethod = nullptr;
		PrepareWaveformMethod = nullptr;
		PlayWaveformMethod = nullptr;
		PlayControlledWaveformMethod = nullptr;
		PauseControlledWaveformMethod = nullptr;
		ResumeControlledWaveformMethod = nullptr;
		SeekControlledWaveformMethod = nullptr;
		StopControlledWaveformMethod = nullptr;
		PlayPredefinedMethod = nullptr;
		PlayPrimitivesMethod = nullptr;
		PlayEnvelopeMethod = nullptr;
		StopAllMethod = nullptr;
		CancelScheduledMethod = nullptr;
		ReleasePreparedResourcesMethod = nullptr;
		return false;
	}
	return true;
}

FOpenMobileHapticsAndroidHardwareProbe
FOpenMobileHapticsAndroidBridge::QueryHardware()
{
	FOpenMobileHapticsAndroidHardwareProbe Probe;
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return Probe;
	}

	FScopedJavaObject<jlongArray> Values(static_cast<jlongArray>(
		Env->CallStaticObjectMethod(
			BridgeClass,
			QueryCapabilitiesMethod,
			Activity
		)
	));
	if (Env->ExceptionCheck() || !Values)
	{
		ClearException(Env);
		return Probe;
	}
	const jsize Count = Env->GetArrayLength(*Values);
	if (Env->ExceptionCheck() || Count < 1)
	{
		ClearException(Env);
		return Probe;
	}
	jlong NativeValues[9] = {-1, 0, 0, -1, -1, -1, -1, -1, -1};
	Env->GetLongArrayRegion(
		*Values,
		0,
		FMath::Min<jsize>(Count, 9),
		NativeValues
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return Probe;
	}
	Probe.Flags = static_cast<int64>(NativeValues[0]);
	Probe.PresetSupport = static_cast<uint64>(NativeValues[1]);
	Probe.PrimitiveSupport = static_cast<uint64>(NativeValues[2]);
	Probe.MaximumControlPointCount = static_cast<int64>(NativeValues[3]);
	Probe.MaximumDurationMillis = static_cast<int64>(NativeValues[4]);
	Probe.MinimumTimingMillis = static_cast<int64>(NativeValues[5]);
	Probe.MaximumControlPointDurationMillis =
		static_cast<int64>(NativeValues[6]);
	Probe.MinimumFrequencyMilliHertz = static_cast<int64>(NativeValues[7]);
	Probe.MaximumFrequencyMilliHertz = static_cast<int64>(NativeValues[8]);
	return Probe;
}

FOpenMobileHapticsAndroidBridgeSubmission
FOpenMobileHapticsAndroidBridge::PlaySemantic(
	const FOpenMobileHapticsBackendRequestToken& Token,
	EOpenMobileHapticsSemanticBehavior Behavior,
	float Intensity,
	EOpenMobileHapticsSemanticPath Path,
	int32 Purpose,
	int64 StartDelayMilliseconds,
	TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> ScheduledStartGuard,
	FName PatternOrEffect,
	FName Channel,
	FName ResolvedPath,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	FOpenMobileHapticsAndroidBridgeSubmission Submission;
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return Submission;
	}

	FPendingCallback Pending;
	Pending.Token = Token;
	Pending.ScheduledStartGuard = MoveTemp(ScheduledStartGuard);
	Pending.PatternOrEffect = PatternOrEffect;
	Pending.Channel = Channel;
	Pending.ResolvedPath = ResolvedPath;
	Pending.Callback = MoveTemp(Callback);
	PendingCallbacks.Add(Token.RequestId, MoveTemp(Pending));
	Submission.Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PlaySemanticMethod,
		Activity,
		static_cast<jlong>(Token.RequestId),
		static_cast<jint>(Behavior),
		static_cast<jfloat>(Intensity),
		static_cast<jint>(Path),
		static_cast<jint>(Purpose),
		static_cast<jlong>(StartDelayMilliseconds)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		Submission.Result = 0;
	}
	Submission.bExpectsCallback = Submission.Result
		== OpenMobileHapticsAndroidBridgePrivate::ResultPending;
	if (!Submission.bExpectsCallback)
	{
		PendingCallbacks.Remove(Token.RequestId);
	}
	return Submission;
}

void FOpenMobileHapticsAndroidBridge::RegisterScheduledCallback(
	const FOpenMobileHapticsBackendRequestToken& Token,
	int32 Result,
	FOpenMobileHapticsAndroidScheduledPlayback&& Scheduled
)
{
	if (Result != OpenMobileHapticsAndroidBridgePrivate::ResultPending
		|| Scheduled.StartDelayMilliseconds <= 0)
	{
		return;
	}
	FPendingCallback Pending;
	Pending.Token = Token;
	Pending.ScheduledStartGuard = MoveTemp(Scheduled.ScheduledStartGuard);
	Pending.PatternOrEffect = Scheduled.PatternOrEffect;
	Pending.Channel = Scheduled.Channel;
	Pending.ResolvedPath = Scheduled.ResolvedPath;
	Pending.Callback = MoveTemp(Scheduled.Callback);
	PendingCallbacks.Add(Token.RequestId, MoveTemp(Pending));
}

int32 FOpenMobileHapticsAndroidBridge::PlayOneShot(
	const FOpenMobileHapticsBackendRequestToken& Token,
	int64 DurationMillis,
	float Intensity,
	EOpenMobileHapticsOneShotPath Path,
	int32 Purpose,
	FOpenMobileHapticsAndroidScheduledPlayback Scheduled
)
{
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return 0;
	}
	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PlayOneShotMethod,
		Activity,
		static_cast<jlong>(Token.RequestId),
		static_cast<jlong>(DurationMillis),
		static_cast<jfloat>(Intensity),
		static_cast<jint>(Path),
		static_cast<jint>(Purpose),
		static_cast<jlong>(Scheduled.StartDelayMilliseconds)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	RegisterScheduledCallback(Token, Result, MoveTemp(Scheduled));
	return Result;
}

int32 FOpenMobileHapticsAndroidBridge::PlayWaveform(
	const FOpenMobileHapticsBackendRequestToken& Token,
	uint64 PreparedResourceId,
	const TArray<int64>& TimingsMilliseconds,
	const TArray<int32>& Amplitudes,
	int32 RepeatIndex,
	int32 Purpose,
	FOpenMobileHapticsAndroidScheduledPlayback Scheduled
)
{
	const int32 Count = TimingsMilliseconds.Num();
	if (Count <= 0 || Amplitudes.Num() != Count)
	{
		return 0;
	}

	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return 0;
	}

	FScopedJavaObject<jlongArray> TimingValues(Env->NewLongArray(Count));
	FScopedJavaObject<jintArray> AmplitudeValues(Env->NewIntArray(Count));
	if (!TimingValues || !AmplitudeValues || Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}

	TArray<jlong, TInlineAllocator<32>> NativeTimings;
	TArray<jint, TInlineAllocator<32>> NativeAmplitudes;
	NativeTimings.Reserve(Count);
	NativeAmplitudes.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		NativeTimings.Add(static_cast<jlong>(TimingsMilliseconds[Index]));
		NativeAmplitudes.Add(static_cast<jint>(Amplitudes[Index]));
	}
	Env->SetLongArrayRegion(
		*TimingValues,
		0,
		Count,
		NativeTimings.GetData()
	);
	Env->SetIntArrayRegion(
		*AmplitudeValues,
		0,
		Count,
		NativeAmplitudes.GetData()
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}

	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PlayWaveformMethod,
		Activity,
		static_cast<jlong>(Token.RequestId),
		static_cast<jlong>(PreparedResourceId),
		*TimingValues,
		*AmplitudeValues,
		static_cast<jint>(RepeatIndex),
		static_cast<jint>(Purpose),
		static_cast<jlong>(Scheduled.StartDelayMilliseconds)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	RegisterScheduledCallback(Token, Result, MoveTemp(Scheduled));
	return Result;
}

int32 FOpenMobileHapticsAndroidBridge::PlayControlledWaveform(
	const FOpenMobileHapticsBackendRequestToken& Token,
	uint64 PreparedResourceId,
	const TArray<int64>& TimingsMilliseconds,
	const TArray<int32>& Amplitudes,
	int32 RepeatIndex,
	int32 Purpose,
	int64 CompletionDurationMilliseconds,
	FOpenMobileHapticsAndroidScheduledPlayback Scheduled
)
{
	const int32 Count = TimingsMilliseconds.Num();
	if (Count <= 0 || Amplitudes.Num() != Count
		|| Token.RequestId == 0 || !Scheduled.Callback)
	{
		return 0;
	}
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return 0;
	}
	FScopedJavaObject<jlongArray> TimingValues(Env->NewLongArray(Count));
	FScopedJavaObject<jintArray> AmplitudeValues(Env->NewIntArray(Count));
	if (!TimingValues || !AmplitudeValues || Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	TArray<jlong, TInlineAllocator<32>> NativeTimings;
	TArray<jint, TInlineAllocator<32>> NativeAmplitudes;
	NativeTimings.Reserve(Count);
	NativeAmplitudes.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		NativeTimings.Add(static_cast<jlong>(TimingsMilliseconds[Index]));
		NativeAmplitudes.Add(static_cast<jint>(Amplitudes[Index]));
	}
	Env->SetLongArrayRegion(
		*TimingValues, 0, Count, NativeTimings.GetData()
	);
	Env->SetIntArrayRegion(
		*AmplitudeValues, 0, Count, NativeAmplitudes.GetData()
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}

	FPendingCallback Pending;
	Pending.Token = Token;
	Pending.ScheduledStartGuard = MoveTemp(Scheduled.ScheduledStartGuard);
	Pending.PatternOrEffect = Scheduled.PatternOrEffect;
	Pending.Channel = Scheduled.Channel;
	Pending.ResolvedPath = Scheduled.ResolvedPath;
	Pending.Callback = MoveTemp(Scheduled.Callback);
	Pending.bControlledWaveform = true;
	PendingCallbacks.Add(Token.RequestId, MoveTemp(Pending));
	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PlayControlledWaveformMethod,
		Activity,
		static_cast<jlong>(Token.RequestId),
		static_cast<jlong>(PreparedResourceId),
		*TimingValues,
		*AmplitudeValues,
		static_cast<jint>(RepeatIndex),
		static_cast<jint>(Purpose),
		static_cast<jlong>(CompletionDurationMilliseconds),
		static_cast<jlong>(Scheduled.StartDelayMilliseconds)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		PendingCallbacks.Remove(Token.RequestId);
		return 0;
	}
	if (Result != OpenMobileHapticsAndroidBridgePrivate::ResultPending)
	{
		PendingCallbacks.Remove(Token.RequestId);
	}
	return Result;
}

int32 FOpenMobileHapticsAndroidBridge::PauseControlledWaveform(
	uint64 RequestId,
	uint64 Revision
)
{
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	FPendingCallback* Pending = PendingCallbacks.Find(RequestId);
	if (!Env || !Activity || !EnsureInitialized(Env) || !Pending
		|| !Pending->bControlledWaveform)
	{
		return OpenMobileHapticsAndroidBridgePrivate::ResultStale;
	}
	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PauseControlledWaveformMethod,
		Activity,
		static_cast<jlong>(RequestId),
		static_cast<jlong>(Revision)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	if (Result == OpenMobileHapticsAndroidBridgePrivate::ResultAccepted)
	{
		Pending->LastControlRevision = Revision;
	}
	return Result;
}

int32 FOpenMobileHapticsAndroidBridge::UpdateControlledWaveform(
	jmethodID Method,
	uint64 RequestId,
	uint64 Revision,
	const TArray<int64>& TimingsMilliseconds,
	const TArray<int32>& Amplitudes,
	int32 RepeatIndex,
	int32 Purpose,
	int64 CompletionDurationMilliseconds
)
{
	const int32 Count = TimingsMilliseconds.Num();
	if (Count <= 0 || Amplitudes.Num() != Count)
	{
		return 0;
	}
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	FPendingCallback* Pending = PendingCallbacks.Find(RequestId);
	if (!Env || !Activity || !EnsureInitialized(Env) || !Method || !Pending
		|| !Pending->bControlledWaveform)
	{
		return OpenMobileHapticsAndroidBridgePrivate::ResultStale;
	}
	FScopedJavaObject<jlongArray> TimingValues(Env->NewLongArray(Count));
	FScopedJavaObject<jintArray> AmplitudeValues(Env->NewIntArray(Count));
	if (!TimingValues || !AmplitudeValues || Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	TArray<jlong, TInlineAllocator<32>> NativeTimings;
	TArray<jint, TInlineAllocator<32>> NativeAmplitudes;
	NativeTimings.Reserve(Count);
	NativeAmplitudes.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		NativeTimings.Add(static_cast<jlong>(TimingsMilliseconds[Index]));
		NativeAmplitudes.Add(static_cast<jint>(Amplitudes[Index]));
	}
	Env->SetLongArrayRegion(
		*TimingValues, 0, Count, NativeTimings.GetData()
	);
	Env->SetIntArrayRegion(
		*AmplitudeValues, 0, Count, NativeAmplitudes.GetData()
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		Method,
		Activity,
		static_cast<jlong>(RequestId),
		static_cast<jlong>(Revision),
		*TimingValues,
		*AmplitudeValues,
		static_cast<jint>(RepeatIndex),
		static_cast<jint>(Purpose),
		static_cast<jlong>(CompletionDurationMilliseconds)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	if (Result == OpenMobileHapticsAndroidBridgePrivate::ResultAccepted)
	{
		Pending->LastControlRevision = Revision;
	}
	return Result;
}

int32 FOpenMobileHapticsAndroidBridge::ResumeControlledWaveform(
	uint64 RequestId,
	uint64 Revision,
	const TArray<int64>& TimingsMilliseconds,
	const TArray<int32>& Amplitudes,
	int32 RepeatIndex,
	int32 Purpose,
	int64 CompletionDurationMilliseconds
)
{
	return UpdateControlledWaveform(
		ResumeControlledWaveformMethod,
		RequestId,
		Revision,
		TimingsMilliseconds,
		Amplitudes,
		RepeatIndex,
		Purpose,
		CompletionDurationMilliseconds
	);
}

int32 FOpenMobileHapticsAndroidBridge::SeekControlledWaveform(
	uint64 RequestId,
	uint64 Revision,
	const TArray<int64>& TimingsMilliseconds,
	const TArray<int32>& Amplitudes,
	int32 RepeatIndex,
	int32 Purpose,
	int64 CompletionDurationMilliseconds
)
{
	return UpdateControlledWaveform(
		SeekControlledWaveformMethod,
		RequestId,
		Revision,
		TimingsMilliseconds,
		Amplitudes,
		RepeatIndex,
		Purpose,
		CompletionDurationMilliseconds
	);
}

bool FOpenMobileHapticsAndroidBridge::StopControlledWaveform(
	uint64 RequestId
)
{
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !BridgeClass || !StopControlledWaveformMethod)
	{
		return false;
	}
	const jboolean Result = Env->CallStaticBooleanMethod(
		BridgeClass,
		StopControlledWaveformMethod,
		static_cast<jlong>(RequestId)
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return false;
	}
	if (Result == JNI_TRUE)
	{
		PendingCallbacks.Remove(RequestId);
		return true;
	}
	return false;
}

int32 FOpenMobileHapticsAndroidBridge::PrepareWaveform(
	uint64 ResourceId,
	const TArray<int64>& TimingsMilliseconds,
	const TArray<int32>& Amplitudes,
	int32 RepeatIndex,
	int64 EstimatedBytes,
	const FOpenMobileHapticsPreparedResourceLimits& Limits
)
{
	const int32 Count = TimingsMilliseconds.Num();
	if (ResourceId == 0 || Count <= 0 || Amplitudes.Num() != Count)
	{
		return 0;
	}

	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return 0;
	}

	FScopedJavaObject<jlongArray> TimingValues(Env->NewLongArray(Count));
	FScopedJavaObject<jintArray> AmplitudeValues(Env->NewIntArray(Count));
	if (!TimingValues || !AmplitudeValues || Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	TArray<jlong, TInlineAllocator<32>> NativeTimings;
	TArray<jint, TInlineAllocator<32>> NativeAmplitudes;
	NativeTimings.Reserve(Count);
	NativeAmplitudes.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		NativeTimings.Add(static_cast<jlong>(TimingsMilliseconds[Index]));
		NativeAmplitudes.Add(static_cast<jint>(Amplitudes[Index]));
	}
	Env->SetLongArrayRegion(
		*TimingValues,
		0,
		Count,
		NativeTimings.GetData()
	);
	Env->SetIntArrayRegion(
		*AmplitudeValues,
		0,
		Count,
		NativeAmplitudes.GetData()
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}

	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PrepareWaveformMethod,
		Activity,
		static_cast<jlong>(ResourceId),
		*TimingValues,
		*AmplitudeValues,
		static_cast<jint>(RepeatIndex),
		static_cast<jlong>(EstimatedBytes),
		static_cast<jint>(Limits.MaximumCount),
		static_cast<jlong>(Limits.MaximumBytes),
		static_cast<jlong>(FMath::RoundToInt64(
			Limits.IdleLifetimeSeconds * 1000.0
		))
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	return Result;
}

void FOpenMobileHapticsAndroidBridge::ReleasePreparedResources()
{
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !BridgeClass || !ReleasePreparedResourcesMethod)
	{
		return;
	}
	Env->CallStaticVoidMethod(BridgeClass, ReleasePreparedResourcesMethod);
	ClearException(Env);
}

int32 FOpenMobileHapticsAndroidBridge::PlayPredefined(
	const FOpenMobileHapticsBackendRequestToken& Token,
	int32 Effect,
	int32 Purpose,
	FOpenMobileHapticsAndroidScheduledPlayback Scheduled
)
{
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return 0;
	}
	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PlayPredefinedMethod,
		Activity,
		static_cast<jlong>(Token.RequestId),
		static_cast<jint>(Effect),
		static_cast<jint>(Purpose),
		static_cast<jlong>(Scheduled.StartDelayMilliseconds)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	RegisterScheduledCallback(Token, Result, MoveTemp(Scheduled));
	return Result;
}

int32 FOpenMobileHapticsAndroidBridge::PlayPrimitives(
	const FOpenMobileHapticsBackendRequestToken& Token,
	const TArray<EOpenMobileHapticAndroidPrimitive>& Primitives,
	const TArray<float>& Scales,
	const TArray<int32>& DelaysMilliseconds,
	int32 Purpose,
	FOpenMobileHapticsAndroidScheduledPlayback Scheduled
)
{
	const int32 Count = Primitives.Num();
	if (Count <= 0 || Scales.Num() != Count
		|| DelaysMilliseconds.Num() != Count)
	{
		return 0;
	}

	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return 0;
	}

	FScopedJavaObject<jintArray> PrimitiveValues(Env->NewIntArray(Count));
	FScopedJavaObject<jfloatArray> ScaleValues(Env->NewFloatArray(Count));
	FScopedJavaObject<jintArray> DelayValues(Env->NewIntArray(Count));
	if (!PrimitiveValues || !ScaleValues || !DelayValues
		|| Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}

	TArray<jint, TInlineAllocator<16>> NativePrimitives;
	TArray<jfloat, TInlineAllocator<16>> NativeScales;
	TArray<jint, TInlineAllocator<16>> NativeDelays;
	NativePrimitives.Reserve(Count);
	NativeScales.Reserve(Count);
	NativeDelays.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		NativePrimitives.Add(static_cast<jint>(Primitives[Index]));
		NativeScales.Add(static_cast<jfloat>(Scales[Index]));
		NativeDelays.Add(static_cast<jint>(DelaysMilliseconds[Index]));
	}
	Env->SetIntArrayRegion(
		*PrimitiveValues,
		0,
		Count,
		NativePrimitives.GetData()
	);
	Env->SetFloatArrayRegion(
		*ScaleValues,
		0,
		Count,
		NativeScales.GetData()
	);
	Env->SetIntArrayRegion(
		*DelayValues,
		0,
		Count,
		NativeDelays.GetData()
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}

	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PlayPrimitivesMethod,
		Activity,
		static_cast<jlong>(Token.RequestId),
		*PrimitiveValues,
		*ScaleValues,
		*DelayValues,
		static_cast<jint>(Purpose),
		static_cast<jlong>(Scheduled.StartDelayMilliseconds)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	RegisterScheduledCallback(Token, Result, MoveTemp(Scheduled));
	return Result;
}

int32 FOpenMobileHapticsAndroidBridge::PlayEnvelope(
	const FOpenMobileHapticsBackendRequestToken& Token,
	EOpenMobileHapticAndroidPatternFormat Format,
	const TArray<float>& Amplitudes,
	const TArray<float>& ControlValues,
	const TArray<int64>& DurationsMilliseconds,
	int32 Purpose,
	FOpenMobileHapticsAndroidScheduledPlayback Scheduled
)
{
	const int32 Count = Amplitudes.Num();
	if (Count <= 0 || ControlValues.Num() != Count
		|| DurationsMilliseconds.Num() != Count)
	{
		return 0;
	}

	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return 0;
	}

	FScopedJavaObject<jfloatArray> AmplitudeValues(Env->NewFloatArray(Count));
	FScopedJavaObject<jfloatArray> ControlValueArray(Env->NewFloatArray(Count));
	FScopedJavaObject<jlongArray> DurationValues(Env->NewLongArray(Count));
	if (!AmplitudeValues || !ControlValueArray || !DurationValues
		|| Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}

	TArray<jfloat, TInlineAllocator<16>> NativeAmplitudes;
	TArray<jfloat, TInlineAllocator<16>> NativeControlValues;
	TArray<jlong, TInlineAllocator<16>> NativeDurations;
	NativeAmplitudes.Reserve(Count);
	NativeControlValues.Reserve(Count);
	NativeDurations.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		NativeAmplitudes.Add(static_cast<jfloat>(Amplitudes[Index]));
		NativeControlValues.Add(static_cast<jfloat>(ControlValues[Index]));
		NativeDurations.Add(static_cast<jlong>(DurationsMilliseconds[Index]));
	}
	Env->SetFloatArrayRegion(
		*AmplitudeValues,
		0,
		Count,
		NativeAmplitudes.GetData()
	);
	Env->SetFloatArrayRegion(
		*ControlValueArray,
		0,
		Count,
		NativeControlValues.GetData()
	);
	Env->SetLongArrayRegion(
		*DurationValues,
		0,
		Count,
		NativeDurations.GetData()
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}

	const int32 Result = static_cast<int32>(Env->CallStaticIntMethod(
		BridgeClass,
		PlayEnvelopeMethod,
		Activity,
		static_cast<jlong>(Token.RequestId),
		static_cast<jint>(Format),
		*AmplitudeValues,
		*ControlValueArray,
		*DurationValues,
		static_cast<jint>(Purpose),
		static_cast<jlong>(Scheduled.StartDelayMilliseconds)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	RegisterScheduledCallback(Token, Result, MoveTemp(Scheduled));
	return Result;
}

bool FOpenMobileHapticsAndroidBridge::CancelScheduled(uint64 RequestId)
{
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !BridgeClass || !CancelScheduledMethod || RequestId == 0)
	{
		return false;
	}
	const jboolean Result = Env->CallStaticBooleanMethod(
		BridgeClass,
		CancelScheduledMethod,
		static_cast<jlong>(RequestId)
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return false;
	}
	if (Result != JNI_TRUE)
	{
		return false;
	}
	PendingCallbacks.Remove(RequestId);
	return true;
}

bool FOpenMobileHapticsAndroidBridge::StopAll()
{
	FScopeLock Lock(&Mutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity || !EnsureInitialized(Env))
	{
		return false;
	}
	const jboolean Result = Env->CallStaticBooleanMethod(
		BridgeClass,
		StopAllMethod,
		Activity
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return false;
	}
	if (Result == JNI_TRUE)
	{
		PendingCallbacks.Reset();
		return true;
	}
	return false;
}

bool FOpenMobileHapticsAndroidBridge::HandleCanStart(uint64 RequestId) const
{
	FScopeLock Lock(&Mutex);
	const FPendingCallback* Pending = PendingCallbacks.Find(RequestId);
	return Pending
		&& FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(Pending->Token)
		&& Pending->ScheduledStartGuard
		&& Pending->ScheduledStartGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
		);
}

void FOpenMobileHapticsAndroidBridge::HandleBridgeResult(
	uint64 RequestId,
	int32 Result
)
{
	FPendingCallback Pending;
	{
		FScopeLock Lock(&Mutex);
		FPendingCallback* Found = PendingCallbacks.Find(RequestId);
		if (!Found)
		{
			return;
		}
		Pending = MoveTemp(*Found);
		PendingCallbacks.Remove(RequestId);
	}
	if (!Pending.Callback)
	{
		return;
	}

	FOpenMobileHapticsBackendCallback Callback;
	Callback.Token = Pending.Token;
	Callback.Sequence = 1;
	Callback.PreviousSequence = 0;
	Callback.Event.PatternOrEffect = Pending.PatternOrEffect;
	Callback.Event.Channel = Pending.Channel;
	Callback.Event.ResolvedPath = Pending.ResolvedPath;
	Callback.Event.State = Result
		== OpenMobileHapticsAndroidBridgePrivate::ResultStale
		? EOpenMobileHapticPlaybackState::Interrupted
		: Result == OpenMobileHapticsAndroidBridgePrivate::ResultAccepted
		|| Result == OpenMobileHapticsAndroidBridgePrivate::ResultSuppressed
		|| Result == OpenMobileHapticsAndroidBridgePrivate::ResultFallback
		|| Result
			== OpenMobileHapticsAndroidBridgePrivate::ResultDefaultAmplitude
		? EOpenMobileHapticPlaybackState::Completed
		: EOpenMobileHapticPlaybackState::Failed;
	Callback.Event.Evidence = Callback.Event.State
		== EOpenMobileHapticPlaybackState::Completed
		? EOpenMobileHapticEventEvidence::Estimated
		: Callback.Event.State == EOpenMobileHapticPlaybackState::Interrupted
			? EOpenMobileHapticEventEvidence::SchedulerConfirmed
			: EOpenMobileHapticEventEvidence::NativeConfirmed;
	Callback.Event.TimestampSeconds = FPlatformTime::Seconds();
	Pending.Callback(Callback);
}

void FOpenMobileHapticsAndroidBridge::HandleControlledWaveformEvent(
	uint64 RequestId,
	uint64 ControlRevision,
	uint64 EventSequence,
	int32 Event
)
{
	FPendingCallback Pending;
	uint64 CallbackSequence = 0;
	{
		FScopeLock Lock(&Mutex);
		FPendingCallback* Found = PendingCallbacks.Find(RequestId);
		if (!Found || !Found->bControlledWaveform
			|| EventSequence == 0
			|| EventSequence <= Found->LastNativeEventSequence
			|| ControlRevision < Found->LastControlRevision)
		{
			return;
		}
		Found->LastNativeEventSequence = EventSequence;
		if (Found->LastForwardedEventSequence == MAX_uint64)
		{
			return;
		}
		CallbackSequence = ++Found->LastForwardedEventSequence;
		Pending = *Found;
		const bool bTerminal = Event
			!= OpenMobileHapticsAndroidBridgePrivate::ControlledEventStarted;
		if (bTerminal)
		{
			PendingCallbacks.Remove(RequestId);
		}
	}
	if (!Pending.Callback)
	{
		return;
	}
	FOpenMobileHapticsBackendCallback Callback;
	Callback.Token = Pending.Token;
	Callback.Sequence = CallbackSequence;
	Callback.PreviousSequence = CallbackSequence - 1;
	Callback.Event.PatternOrEffect = Pending.PatternOrEffect;
	Callback.Event.Channel = Pending.Channel;
	Callback.Event.ResolvedPath = Pending.ResolvedPath;
	Callback.Event.Evidence =
		EOpenMobileHapticEventEvidence::SchedulerConfirmed;
	switch (Event)
	{
	case OpenMobileHapticsAndroidBridgePrivate::ControlledEventStarted:
		Callback.Event.State = EOpenMobileHapticPlaybackState::Started;
		break;
	case OpenMobileHapticsAndroidBridgePrivate::ControlledEventCompleted:
		Callback.Event.State = EOpenMobileHapticPlaybackState::Completed;
		break;
	case OpenMobileHapticsAndroidBridgePrivate::ControlledEventInterrupted:
		Callback.Event.State = EOpenMobileHapticPlaybackState::Interrupted;
		break;
	case OpenMobileHapticsAndroidBridgePrivate::ControlledEventFailed:
	default:
		Callback.Event.State = EOpenMobileHapticPlaybackState::Failed;
		break;
	}
	Callback.Event.TimestampSeconds = FPlatformTime::Seconds();
	Pending.Callback(Callback);
}

void FOpenMobileHapticsAndroidBridge::Shutdown()
{
	{
		FScopeLock ActiveLock(
			&OpenMobileHapticsAndroidBridgePrivate::ActiveBridgeMutex
		);
		if (OpenMobileHapticsAndroidBridgePrivate::ActiveBridge == this)
		{
			OpenMobileHapticsAndroidBridgePrivate::ActiveBridge = nullptr;
		}
	}
	FScopeLock Lock(&Mutex);
	PendingCallbacks.Reset();
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env && BridgeClass)
	{
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (Activity && StopAllMethod)
		{
			Env->CallStaticBooleanMethod(BridgeClass, StopAllMethod, Activity);
			ClearException(Env);
		}
		Env->CallStaticVoidMethod(BridgeClass, ReleasePreparedResourcesMethod);
		ClearException(Env);
		Env->DeleteGlobalRef(BridgeClass);
	}
	BridgeClass = nullptr;
	QueryCapabilitiesMethod = nullptr;
	PlaySemanticMethod = nullptr;
	PlayOneShotMethod = nullptr;
	PrepareWaveformMethod = nullptr;
	PlayWaveformMethod = nullptr;
	PlayControlledWaveformMethod = nullptr;
	PauseControlledWaveformMethod = nullptr;
	ResumeControlledWaveformMethod = nullptr;
	SeekControlledWaveformMethod = nullptr;
	StopControlledWaveformMethod = nullptr;
	PlayPredefinedMethod = nullptr;
	PlayPrimitivesMethod = nullptr;
	PlayEnvelopeMethod = nullptr;
	StopAllMethod = nullptr;
	CancelScheduledMethod = nullptr;
	ReleasePreparedResourcesMethod = nullptr;
}

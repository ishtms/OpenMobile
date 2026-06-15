#include "OpenMobileHapticsAndroidBridge.h"

#include "Android/AndroidApplication.h"
#include "Misc/ScopeLock.h"

namespace OpenMobileHapticsAndroidBridgePrivate
{
	constexpr int32 ResultAccepted = 1;
	constexpr int32 ResultSuppressed = 2;
	constexpr int32 ResultPending = 6;

	FCriticalSection ActiveBridgeMutex;
	FOpenMobileHapticsAndroidBridge* ActiveBridge = nullptr;
}

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
		"(Landroid/app/Activity;JIFII)I"
	);
	PlayOneShotMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playOneShot",
		"(Landroid/app/Activity;JJFII)I"
	);
	PlayPrimitivesMethod = Env->GetStaticMethodID(
		BridgeClass,
		"playPrimitives",
		"(Landroid/app/Activity;J[I[F[II)I"
	);
	StopAllMethod = Env->GetStaticMethodID(
		BridgeClass,
		"stopAll",
		"(Landroid/app/Activity;)Z"
	);
	if (Env->ExceptionCheck()
		|| !QueryCapabilitiesMethod
		|| !PlaySemanticMethod
		|| !PlayOneShotMethod
		|| !PlayPrimitivesMethod
		|| !StopAllMethod)
	{
		ClearException(Env);
		Env->DeleteGlobalRef(BridgeClass);
		BridgeClass = nullptr;
		QueryCapabilitiesMethod = nullptr;
		PlaySemanticMethod = nullptr;
		PlayOneShotMethod = nullptr;
		PlayPrimitivesMethod = nullptr;
		StopAllMethod = nullptr;
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
	jlong NativeValues[6] = {-1, 0, 0, -1, -1, -1};
	Env->GetLongArrayRegion(
		*Values,
		0,
		FMath::Min<jsize>(Count, 6),
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
	return Probe;
}

FOpenMobileHapticsAndroidBridgeSubmission
FOpenMobileHapticsAndroidBridge::PlaySemantic(
	const FOpenMobileHapticsBackendRequestToken& Token,
	EOpenMobileHapticsSemanticBehavior Behavior,
	float Intensity,
	EOpenMobileHapticsSemanticPath Path,
	int32 Purpose,
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
		static_cast<jint>(Purpose)
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

int32 FOpenMobileHapticsAndroidBridge::PlayOneShot(
	const FOpenMobileHapticsBackendRequestToken& Token,
	int64 DurationMillis,
	float Intensity,
	EOpenMobileHapticsOneShotPath Path,
	int32 Purpose
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
		static_cast<jint>(Purpose)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	return Result;
}

int32 FOpenMobileHapticsAndroidBridge::PlayPrimitives(
	const FOpenMobileHapticsBackendRequestToken& Token,
	const TArray<EOpenMobileHapticAndroidPrimitive>& Primitives,
	const TArray<float>& Scales,
	const TArray<int32>& DelaysMilliseconds,
	int32 Purpose
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
		static_cast<jint>(Purpose)
	));
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return 0;
	}
	return Result;
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
	return Result == JNI_TRUE;
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
	Callback.Event.PatternOrEffect = Pending.PatternOrEffect;
	Callback.Event.Channel = Pending.Channel;
	Callback.Event.ResolvedPath = Pending.ResolvedPath;
	Callback.Event.Evidence = EOpenMobileHapticEventEvidence::SchedulerConfirmed;
	Callback.Event.State = Result
		== OpenMobileHapticsAndroidBridgePrivate::ResultAccepted
		|| Result == OpenMobileHapticsAndroidBridgePrivate::ResultSuppressed
		? EOpenMobileHapticPlaybackState::Completed
		: EOpenMobileHapticPlaybackState::Failed;
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
		Env->DeleteGlobalRef(BridgeClass);
	}
	BridgeClass = nullptr;
	QueryCapabilitiesMethod = nullptr;
	PlaySemanticMethod = nullptr;
	PlayOneShotMethod = nullptr;
	PlayPrimitivesMethod = nullptr;
	StopAllMethod = nullptr;
}

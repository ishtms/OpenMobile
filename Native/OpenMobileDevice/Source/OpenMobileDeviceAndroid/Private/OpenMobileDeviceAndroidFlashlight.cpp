#include "OpenMobileDeviceAndroidFlashlight.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMonitoringService.h"

namespace OpenMobileDeviceAndroidFlashlightPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;

	bool ClearJavaException(JNIEnv* Env)
	{
		if (!Env->ExceptionCheck())
		{
			return false;
		}
		Env->ExceptionClear();
		return true;
	}

	bool CallStringArrayMethod(TArray<FString>& OutValues)
	{
		OutValues.Reset();
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Env || !Activity)
		{
			return false;
		}
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID Method = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"AndroidThunkJava_OpenMobileDeviceGetFlashlightDetails",
				"()[Ljava/lang/String;"
			)
			: nullptr;
		if (!Method || ClearJavaException(Env))
		{
			return false;
		}
		FScopedJavaObject<jobjectArray> Values(static_cast<jobjectArray>(
			Env->CallObjectMethod(Activity, Method)
		));
		if (!Values || ClearJavaException(Env))
		{
			return false;
		}
		const jsize Count = Env->GetArrayLength(*Values);
		if (ClearJavaException(Env))
		{
			return false;
		}
		OutValues.Reserve(Count);
		for (jsize Index = 0; Index < Count; ++Index)
		{
			jstring Value = static_cast<jstring>(
				Env->GetObjectArrayElement(*Values, Index)
			);
			if (ClearJavaException(Env))
			{
				OutValues.Reset();
				return false;
			}
			OutValues.Add(FJavaHelper::FStringFromLocalRef(Env, Value));
		}
		return true;
	}

	void ClearState()
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		SourceSequence = 0;
	}

	void NotifyChange()
	{
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 Sequence = 0;
		{
			FScopeLock Lock(&StateMutex);
			if (!ActiveToken.IsValid())
			{
				return;
			}
			SourceSequence = SourceSequence == MAX_uint64
				? 1
				: SourceSequence + 1;
			CallbackToken = ActiveToken;
			Sequence = SourceSequence;
		}
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(
			CallbackToken,
			Sequence
		);
	}

	bool CallMonitoringMethod(const char* MethodName)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Env || !Activity)
		{
			return false;
		}
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID Method = ActivityClass
			? Env->GetMethodID(*ActivityClass, MethodName, "()Z")
			: nullptr;
		if (!Method || ClearJavaException(Env))
		{
			return false;
		}
		const bool bResult = Env->CallBooleanMethod(Activity, Method);
		return !ClearJavaException(Env) && bResult;
	}
}

FOpenMobileFlashlightSnapshot GetOpenMobileDeviceAndroidFlashlightSnapshot()
{
	using namespace OpenMobileDeviceAndroidFlashlightPrivate;
	FOpenMobileFlashlightSnapshot Snapshot;
	TArray<FString> Values;
	if (!CallStringArrayMethod(Values) || Values.Num() != 8)
	{
		return Snapshot;
	}

	Snapshot.HardwareState = Values[0] == TEXT("available")
		? EOpenMobileFlashlightHardwareState::Available
		: Values[0] == TEXT("unavailable")
			? EOpenMobileFlashlightHardwareState::Unavailable
			: EOpenMobileFlashlightHardwareState::Unknown;
	Snapshot.TorchState = Values[1] == TEXT("on")
		? EOpenMobileFlashlightTorchState::On
		: Values[1] == TEXT("off")
			? EOpenMobileFlashlightTorchState::Off
			: EOpenMobileFlashlightTorchState::Unknown;
	if (!Values[2].IsEmpty())
	{
		const float Intensity = FCString::Atof(*Values[2]);
		if (FMath::IsFinite(Intensity) && Intensity >= 0.0f
			&& Intensity <= 1.0f)
		{
			Snapshot.CurrentIntensity =
				FOpenMobileDeviceOptionalFloat::MakeAvailable(Intensity);
		}
	}
	if (Values[3] == TEXT("true") || Values[3] == TEXT("false"))
	{
		Snapshot.bVariableIntensitySupported =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				Values[3] == TEXT("true")
			);
	}
	Snapshot.PermissionState = Values[4] == TEXT("not_required")
		? EOpenMobileFlashlightPermissionState::NotRequired
		: EOpenMobileFlashlightPermissionState::Unknown;
	Snapshot.ConflictState = Values[5] == TEXT("none")
		? EOpenMobileFlashlightConflictState::None
		: Values[5] == TEXT("busy")
			? EOpenMobileFlashlightConflictState::CameraResourceBusy
			: EOpenMobileFlashlightConflictState::Unknown;
	Snapshot.ThermalState = Values[6] == TEXT("not_restricted")
		? EOpenMobileFlashlightThermalState::NotRestricted
		: Values[6] == TEXT("restricted")
			? EOpenMobileFlashlightThermalState::Restricted
			: EOpenMobileFlashlightThermalState::Unknown;
	Snapshot.Ownership = Values[7] == TEXT("application")
		? EOpenMobileFlashlightOwnership::ThisApplication
		: Values[7] == TEXT("external")
			? EOpenMobileFlashlightOwnership::External
			: EOpenMobileFlashlightOwnership::Unknown;
	return Snapshot;
}

bool StartOpenMobileDeviceAndroidFlashlightMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	using namespace OpenMobileDeviceAndroidFlashlightPrivate;
	check(IsInGameThread());
	if (!CallbackToken.IsValid())
	{
		return false;
	}
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = CallbackToken;
		SourceSequence = 0;
	}
	if (!CallMonitoringMethod(
		"AndroidThunkJava_OpenMobileDeviceStartFlashlightMonitoring"
	))
	{
		ClearState();
		return false;
	}
	return true;
}

void StopOpenMobileDeviceAndroidFlashlightMonitoring()
{
	using namespace OpenMobileDeviceAndroidFlashlightPrivate;
	check(IsInGameThread());
	ClearState();
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		return;
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceStopFlashlightMonitoring",
			"()V"
		)
		: nullptr;
	if (!Method || ClearJavaException(Env))
	{
		return;
	}
	Env->CallVoidMethod(Activity, Method);
	ClearJavaException(Env);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileDeviceFlashlightChanged(
	JNIEnv* Env,
	jobject Activity
)
{
	static_cast<void>(Env);
	static_cast<void>(Activity);
	OpenMobileDeviceAndroidFlashlightPrivate::NotifyChange();
}

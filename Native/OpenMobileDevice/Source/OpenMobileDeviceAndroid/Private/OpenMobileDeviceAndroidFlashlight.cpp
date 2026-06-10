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

	bool CallOperationMethod(
		const FOpenMobileFlashlightRequest& Request,
		TArray<FString>& OutValues
	)
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
				"AndroidThunkJava_OpenMobileDeviceApplyFlashlight",
				"(IF)[Ljava/lang/String;"
			)
			: nullptr;
		if (!Method || ClearJavaException(Env))
		{
			return false;
		}
		FScopedJavaObject<jobjectArray> Values(static_cast<jobjectArray>(
			Env->CallObjectMethod(
				Activity,
				Method,
				static_cast<jint>(Request.Operation),
				static_cast<jfloat>(Request.Intensity)
			)
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

	void ApplyOptionalFloat(
		const FString& Text,
		FOpenMobileDeviceOptionalFloat& OutValue
	)
	{
		if (Text.IsEmpty())
		{
			return;
		}
		const float Value = FCString::Atof(*Text);
		if (FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f)
		{
			OutValue = FOpenMobileDeviceOptionalFloat::MakeAvailable(Value);
		}
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
	if (!CallStringArrayMethod(Values) || Values.Num() != 10)
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
	ApplyOptionalFloat(Values[2], Snapshot.CurrentIntensity);
	if (Values[3] == TEXT("true") || Values[3] == TEXT("false"))
	{
		Snapshot.bVariableIntensitySupported =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				Values[3] == TEXT("true")
			);
	}
	ApplyOptionalFloat(Values[4], Snapshot.MinimumIntensity);
	ApplyOptionalFloat(Values[5], Snapshot.MaximumIntensity);
	Snapshot.PermissionState = Values[6] == TEXT("not_required")
		? EOpenMobileFlashlightPermissionState::NotRequired
		: EOpenMobileFlashlightPermissionState::Unknown;
	Snapshot.ConflictState = Values[7] == TEXT("none")
		? EOpenMobileFlashlightConflictState::None
		: Values[7] == TEXT("busy")
			? EOpenMobileFlashlightConflictState::CameraResourceBusy
			: EOpenMobileFlashlightConflictState::Unknown;
	Snapshot.ThermalState = Values[8] == TEXT("not_restricted")
		? EOpenMobileFlashlightThermalState::NotRestricted
		: Values[8] == TEXT("restricted")
			? EOpenMobileFlashlightThermalState::Restricted
			: EOpenMobileFlashlightThermalState::Unknown;
	Snapshot.Ownership = Values[9] == TEXT("application")
		? EOpenMobileFlashlightOwnership::ThisApplication
		: Values[9] == TEXT("external")
			? EOpenMobileFlashlightOwnership::External
			: EOpenMobileFlashlightOwnership::Unknown;
	return Snapshot;
}

FOpenMobileFlashlightOperationResult ApplyOpenMobileDeviceAndroidFlashlight(
	const FOpenMobileFlashlightRequest& Request
)
{
	using namespace OpenMobileDeviceAndroidFlashlightPrivate;
	FOpenMobileFlashlightOperationResult Result;
	Result.Request = Request;
	Result.PermissionState = EOpenMobileFlashlightPermissionState::NotRequired;
	TArray<FString> Values;
	if (!CallOperationMethod(Request, Values) || Values.Num() != 5)
	{
		Result.State = EOpenMobileFlashlightOperationState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not run the flashlight operation."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}

	if (Values[0] == TEXT("applied"))
	{
		Result.State = EOpenMobileFlashlightOperationState::Applied;
	}
	else if (Values[0] == TEXT("unsupported"))
	{
		Result.State = EOpenMobileFlashlightOperationState::Unsupported;
	}
	else if (Values[0] == TEXT("busy"))
	{
		Result.State = EOpenMobileFlashlightOperationState::Busy;
	}
	else if (Values[0] == TEXT("permission_required"))
	{
		Result.State = EOpenMobileFlashlightOperationState::PermissionRequired;
		Result.PermissionState =
			EOpenMobileFlashlightPermissionState::NotDetermined;
	}
	else if (Values[0] == TEXT("permission_denied"))
	{
		Result.State = EOpenMobileFlashlightOperationState::PermissionDenied;
		Result.PermissionState = EOpenMobileFlashlightPermissionState::Denied;
	}
	else if (Values[0] == TEXT("restricted"))
	{
		Result.State = EOpenMobileFlashlightOperationState::Restricted;
	}
	else
	{
		Result.State = EOpenMobileFlashlightOperationState::Rejected;
	}
	Result.EffectiveTorchState = Values[1] == TEXT("on")
		? EOpenMobileFlashlightTorchState::On
		: Values[1] == TEXT("off")
			? EOpenMobileFlashlightTorchState::Off
			: EOpenMobileFlashlightTorchState::Unknown;
	ApplyOptionalFloat(Values[2], Result.EffectiveIntensity);
	if (!Result.IsApplied())
	{
		const EOpenMobileErrorCode ErrorCode =
			Result.State == EOpenMobileFlashlightOperationState::Unsupported
				? EOpenMobileErrorCode::NotSupported
				: Result.State == EOpenMobileFlashlightOperationState::Busy
					? EOpenMobileErrorCode::Busy
					: EOpenMobileErrorCode::Unavailable;
		Result.Error = FOpenMobileError::Make(
			ErrorCode,
			TEXT("Android rejected the flashlight operation."),
			Values[4],
			TEXT("Android")
		);
	}
	return Result;
}

void ClearOpenMobileDeviceAndroidFlashlight()
{
	FOpenMobileFlashlightRequest Request;
	Request.Operation = EOpenMobileFlashlightOperation::Off;
	ApplyOpenMobileDeviceAndroidFlashlight(Request);
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

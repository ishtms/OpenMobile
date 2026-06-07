#include "OpenMobileDeviceAndroidSystemUiControl.h"

#include "Android/AndroidApplication.h"

namespace OpenMobileDeviceAndroidSystemUiControlPrivate
{
	bool ClearJavaException(JNIEnv* Env)
	{
		if (!Env->ExceptionCheck())
		{
			return false;
		}
		Env->ExceptionClear();
		return true;
	}
}

FOpenMobileSystemUiResult ApplyOpenMobileDeviceAndroidSystemUiMode(
	const FOpenMobileSystemUiRequest& Request
)
{
	using namespace OpenMobileDeviceAndroidSystemUiControlPrivate;
	FOpenMobileSystemUiResult Result;
	Result.Request = Request;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		Result.State = EOpenMobileSystemUiApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity is unavailable for the system UI request."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceApplySystemUiMode",
			"(I)[I"
		)
		: nullptr;
	if (!Method || ClearJavaException(Env))
	{
		Result.State = EOpenMobileSystemUiApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android system UI method is unavailable."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	jintArray Values = static_cast<jintArray>(Env->CallObjectMethod(
		Activity,
		Method,
		static_cast<jint>(Request.Mode)
	));
	if (ClearJavaException(Env) || !Values || Env->GetArrayLength(Values) < 1)
	{
		if (Values)
		{
			Env->DeleteLocalRef(Values);
		}
		Result.State = EOpenMobileSystemUiApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not apply the active-window system UI mode."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	jint EffectiveMode = -1;
	Env->GetIntArrayRegion(Values, 0, 1, &EffectiveMode);
	Env->DeleteLocalRef(Values);
	if (ClearJavaException(Env) || EffectiveMode < 0 || EffectiveMode > 2)
	{
		Result.State = EOpenMobileSystemUiApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android returned an invalid effective system UI mode."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	Result.State = EOpenMobileSystemUiApplyState::Applied;
	Result.bEffectiveModeAvailable = true;
	Result.EffectiveMode = static_cast<EOpenMobileSystemUiMode>(EffectiveMode);
	return Result;
}

void ClearOpenMobileDeviceAndroidSystemUiMode()
{
	using namespace OpenMobileDeviceAndroidSystemUiControlPrivate;
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
			"AndroidThunkJava_OpenMobileDeviceClearSystemUiMode",
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

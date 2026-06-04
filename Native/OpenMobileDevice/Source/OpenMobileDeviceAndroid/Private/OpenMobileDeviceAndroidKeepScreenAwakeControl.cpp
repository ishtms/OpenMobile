#include "OpenMobileDeviceAndroidKeepScreenAwakeControl.h"

#include "Android/AndroidApplication.h"

namespace OpenMobileDeviceAndroidKeepScreenAwakeControlPrivate
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

FOpenMobileKeepScreenAwakeResult
ApplyOpenMobileDeviceAndroidKeepScreenAwake()
{
	using namespace OpenMobileDeviceAndroidKeepScreenAwakeControlPrivate;
	FOpenMobileKeepScreenAwakeResult Result;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		Result.State = EOpenMobileKeepScreenAwakeApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity is unavailable for the keep-awake request."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceApplyKeepScreenAwake",
			"()Z"
		)
		: nullptr;
	if (!Method || ClearJavaException(Env))
	{
		Result.State = EOpenMobileKeepScreenAwakeApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android keep-awake method is unavailable."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	const bool bApplied = Env->CallBooleanMethod(Activity, Method) == JNI_TRUE;
	const bool bCallError = ClearJavaException(Env);
	Result.State = bApplied && !bCallError
		? EOpenMobileKeepScreenAwakeApplyState::Applied
		: EOpenMobileKeepScreenAwakeApplyState::Rejected;
	if (Result.IsAccepted())
	{
		Result.bEffectiveKeepScreenAwake =
			FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	}
	else
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not apply the active-window keep-awake flag."),
			FString(),
			TEXT("Android")
		);
	}
	return Result;
}

void ClearOpenMobileDeviceAndroidKeepScreenAwake()
{
	using namespace OpenMobileDeviceAndroidKeepScreenAwakeControlPrivate;
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
			"AndroidThunkJava_OpenMobileDeviceClearKeepScreenAwake",
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

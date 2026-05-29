#include "OpenMobileDeviceAndroidOrientationControl.h"

#include "Android/AndroidApplication.h"

namespace OpenMobileDeviceAndroidOrientationControlPrivate
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

FOpenMobileOrientationPolicyResult
ApplyOpenMobileDeviceAndroidOrientationPolicy(
	const FOpenMobileOrientationPolicyRequest& Request
)
{
	FOpenMobileOrientationPolicyResult Result;
	Result.Request = Request;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		Result.State = EOpenMobileOrientationPolicyApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity is unavailable for the orientation request."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}

	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceApplyOrientationPolicy",
			"(I)Z"
		)
		: nullptr;
	if (!Method
		|| OpenMobileDeviceAndroidOrientationControlPrivate::
			ClearJavaException(Env))
	{
		Result.State = EOpenMobileOrientationPolicyApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android orientation request method is unavailable."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}

	const bool bAccepted = Env->CallBooleanMethod(
		Activity,
		Method,
		static_cast<jint>(Request.Policy)
	) == JNI_TRUE;
	const bool bCallError =
		OpenMobileDeviceAndroidOrientationControlPrivate::ClearJavaException(Env);
	Result.State = bAccepted && !bCallError
		? EOpenMobileOrientationPolicyApplyState::Accepted
		: EOpenMobileOrientationPolicyApplyState::Rejected;
	if (!Result.IsAccepted())
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android rejected the orientation request."),
			FString(),
			TEXT("Android")
		);
	}
	return Result;
}

void ClearOpenMobileDeviceAndroidOrientationPolicy()
{
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
			"AndroidThunkJava_OpenMobileDeviceClearOrientationPolicy",
			"()V"
		)
		: nullptr;
	if (!Method
		|| OpenMobileDeviceAndroidOrientationControlPrivate::
			ClearJavaException(Env))
	{
		return;
	}
	Env->CallVoidMethod(Activity, Method);
	OpenMobileDeviceAndroidOrientationControlPrivate::ClearJavaException(Env);
}

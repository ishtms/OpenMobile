#include "OpenMobileDeviceAndroidApplicationSettings.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

namespace OpenMobileDeviceAndroidApplicationSettingsPrivate
{
	FOpenMobileApplicationSettingsOpenResult MakeResult(
		EOpenMobileApplicationSettingsOpenState State,
		EOpenMobileErrorCode ErrorCode,
		const TCHAR* Message
	)
	{
		FOpenMobileApplicationSettingsOpenResult Result;
		Result.State = State;
		Result.Error = FOpenMobileError::Make(
			ErrorCode,
			Message,
			FString(),
			TEXT("Android")
		);
		return Result;
	}
}

FOpenMobileApplicationSettingsOpenResult
OpenOpenMobileDeviceAndroidApplicationSettings()
{
	using namespace OpenMobileDeviceAndroidApplicationSettingsPrivate;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		return MakeResult(
			EOpenMobileApplicationSettingsOpenState::NoPresenter,
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity is unavailable for application settings.")
		);
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceOpenApplicationSettings",
			"()I"
		)
		: nullptr;
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
	}
	if (!Method)
	{
		return MakeResult(
			EOpenMobileApplicationSettingsOpenState::NativeFailure,
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android application-settings bridge is unavailable.")
		);
	}
	const jint NativeResult = Env->CallIntMethod(Activity, Method);
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		return MakeResult(
			EOpenMobileApplicationSettingsOpenState::NativeFailure,
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not submit the application-settings request.")
		);
	}
	FOpenMobileApplicationSettingsOpenResult Result;
	if (NativeResult == 1)
	{
		Result.State = EOpenMobileApplicationSettingsOpenState::Accepted;
	}
	else if (NativeResult == 0)
	{
		Result = MakeResult(
			EOpenMobileApplicationSettingsOpenState::Unsupported,
			EOpenMobileErrorCode::NotSupported,
			TEXT("Android has no application-details settings activity.")
		);
	}
	else if (NativeResult == 2)
	{
		Result = MakeResult(
			EOpenMobileApplicationSettingsOpenState::NoPresenter,
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity cannot present application settings.")
		);
	}
	else
	{
		Result = MakeResult(
			EOpenMobileApplicationSettingsOpenState::NativeFailure,
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android rejected the application-settings request.")
		);
	}
	return Result;
}

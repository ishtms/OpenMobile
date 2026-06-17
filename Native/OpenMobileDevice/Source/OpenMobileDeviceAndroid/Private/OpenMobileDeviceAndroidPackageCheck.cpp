#include "OpenMobileDeviceAndroidPackageCheck.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

FOpenMobileAndroidPackageCheckResult CheckOpenMobileDeviceAndroidPackage(
	const FOpenMobileAndroidPackageCheckRequest& Request
)
{
	FOpenMobileAndroidPackageCheckResult Result;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		Result.State = EOpenMobileAndroidPackageCheckState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity is unavailable for a package check."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceCheckPackage",
			"(Ljava/lang/String;)I"
		)
		: nullptr;
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
	}
	if (!Method)
	{
		Result.State = EOpenMobileAndroidPackageCheckState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android package-check bridge is unavailable."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	FScopedJavaObject<jstring> PackageName = FJavaHelper::ToJavaString(
		Env,
		Request.PackageName
	);
	const jint NativeResult = Env->CallIntMethod(
		Activity,
		Method,
		*PackageName
	);
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		Result.State = EOpenMobileAndroidPackageCheckState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not complete the declared package check."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	if (NativeResult == 1)
	{
		Result.State = EOpenMobileAndroidPackageCheckState::Installed;
	}
	else if (NativeResult == 2)
	{
		Result.State = EOpenMobileAndroidPackageCheckState::Disabled;
	}
	else if (NativeResult == 0)
	{
		Result.State =
			EOpenMobileAndroidPackageCheckState::NotFoundOrNotVisible;
	}
	else
	{
		Result.State = EOpenMobileAndroidPackageCheckState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android rejected the declared package check."),
			FString(),
			TEXT("Android")
		);
	}
	return Result;
}

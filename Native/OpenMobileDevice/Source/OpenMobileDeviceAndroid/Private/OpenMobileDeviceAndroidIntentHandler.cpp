#include "OpenMobileDeviceAndroidIntentHandler.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

FOpenMobileIntentHandlerCheckResult
CheckOpenMobileDeviceAndroidIntentHandler(
	const FOpenMobileIntentHandlerCheckRequest& Request
)
{
	FOpenMobileIntentHandlerCheckResult Result;
	Result.Kind = Request.Kind;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		Result.State = EOpenMobileIntentHandlerCheckState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity is unavailable for a handler check."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceCheckIntentHandler",
			"(Ljava/lang/String;Ljava/lang/String;I)I"
		)
		: nullptr;
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
	}
	if (!Method)
	{
		Result.State = EOpenMobileIntentHandlerCheckState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android handler-check bridge is unavailable."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	FScopedJavaObject<jstring> Url = FJavaHelper::ToJavaString(
		Env,
		Request.Url
	);
	FScopedJavaObject<jstring> Action = FJavaHelper::ToJavaString(
		Env,
		Request.DeclaredIntentAction
	);
	const jint NativeResult = Env->CallIntMethod(
		Activity,
		Method,
		*Url,
		*Action,
		static_cast<jint>(Request.Kind)
	);
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		Result.State = EOpenMobileIntentHandlerCheckState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not resolve the declared handler query."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	if (NativeResult == 1)
	{
		Result.State = EOpenMobileIntentHandlerCheckState::CanHandle;
	}
	else if (NativeResult == 0)
	{
		Result.State = EOpenMobileIntentHandlerCheckState::CannotHandle;
	}
	else
	{
		Result.State = EOpenMobileIntentHandlerCheckState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android rejected the declared handler query."),
			FString(),
			TEXT("Android")
		);
	}
	return Result;
}

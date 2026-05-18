#include "OpenMobileDeviceAndroidStorage.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OpenMobileDeviceStorageInfo.h"

bool QueryOpenMobileDeviceAndroidStorage(
	FOpenMobileStorageSnapshot& OutSnapshot,
	FOpenMobileError& OutError
)
{
	OutSnapshot = {};
	OutError = {};
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity is unavailable for the storage query."),
			FString(),
			TEXT("Android")
		);
		return false;
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceGetStorageSpace",
			"()[Ljava/lang/String;"
		)
		: nullptr;
	const bool bMethodError = Env->ExceptionCheck();
	if (bMethodError)
	{
		Env->ExceptionClear();
	}
	if (!Method || bMethodError)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android storage query method is unavailable."),
			FString(),
			TEXT("Android")
		);
		return false;
	}
	FScopedJavaObject<jobjectArray> Values(static_cast<jobjectArray>(
		Env->CallObjectMethod(Activity, Method)
	));
	const bool bCallError = Env->ExceptionCheck();
	if (bCallError)
	{
		Env->ExceptionClear();
	}
	if (!Values || bCallError || Env->GetArrayLength(*Values) != 2)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android could not read the application data volume."),
			FString(),
			TEXT("Android")
		);
		return false;
	}
	jstring TotalValue = static_cast<jstring>(
		Env->GetObjectArrayElement(*Values, 0)
	);
	jstring AvailableValue = static_cast<jstring>(
		Env->GetObjectArrayElement(*Values, 1)
	);
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android returned invalid application volume values."),
			FString(),
			TEXT("Android")
		);
		return false;
	}
	const FString Total = FJavaHelper::FStringFromLocalRef(Env, TotalValue);
	const FString Available =
		FJavaHelper::FStringFromLocalRef(Env, AvailableValue);
	const bool bValuesValid = Total.IsNumeric() && Available.IsNumeric();
	OutSnapshot = FOpenMobileDeviceStorageInfo::Build(
		EOpenMobileStorageScope::ApplicationDataVolume,
		bValuesValid ? FCString::Strtoui64(*Total, nullptr, 10) : 0,
		bValuesValid ? FCString::Strtoui64(*Available, nullptr, 10) : 0,
		false,
		0,
		bValuesValid
	);
	if (!OutSnapshot.TotalBytes.bIsAvailable
		|| !OutSnapshot.AvailableBytes.bIsAvailable)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android returned unusable application volume values."),
			FString(),
			TEXT("Android")
		);
		return false;
	}
	return true;
}

#include "OpenMobileMediaAndroidBackend.h"

#include "OpenMobileMediaPlatform.h"

#if PLATFORM_ANDROID

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

bool FOpenMobileMediaAndroidBackend::LaunchPhotoPicker(int64 RequestId, FString& OutError)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID OpenPickerMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobilePhotoPicker",
		"(J)Z",
		false
	);

	if (!OpenPickerMethod)
	{
		OutError = TEXT("The Android photo picker bridge was not packaged into GameActivity.");
		return false;
	}

	const bool bLaunched = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		OpenPickerMethod,
		static_cast<jlong>(RequestId)
	);

	if (!bLaunched)
	{
		OutError = TEXT("Android could not schedule the system photo picker.");
	}

	return bLaunched;
}

void FOpenMobileMediaAndroidBackend::CancelPhotoPicker(int64 RequestId)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID CancelMethod = FJavaWrapper::FindMethod(Env,
			FJavaWrapper::GameActivityClassID, "AndroidThunkJava_CancelOpenMobilePhotoPicker", "(J)V", false);
		if (CancelMethod)
		{
			FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis,
				CancelMethod, static_cast<jlong>(RequestId));
		}
	}
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobilePhotoPicked(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring CachedImagePath,
	jstring MetadataJson
)
{
	FOpenMobileMediaPlatform::NativePicked(
		RequestId,
		FJavaHelper::FStringFromParam(Env, CachedImagePath),
		FJavaHelper::FStringFromParam(Env, MetadataJson)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobilePhotoPickCancelled(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileMediaPlatform::NativeCancelled(RequestId);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobilePhotoPickFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorMessage
)
{
	FOpenMobileMediaPlatform::NativeError(RequestId, FJavaHelper::FStringFromParam(Env, ErrorMessage));
}

#endif

#include "OpenMobileAdsAdMobAndroidBackend.h"

#include "OpenMobileAdsAdMobPlatform.h"

#if PLATFORM_ANDROID

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

void FOpenMobileAdsAdMobAndroidBackend::Initialize()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		return;
	}

	static jmethodID InitializeMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_InitializeOpenMobileRewardedAds",
		"()V",
		false
	);

	if (InitializeMethod)
	{
		FJavaWrapper::CallVoidMethod(
			Env,
			FJavaWrapper::GameActivityThis,
			InitializeMethod
		);
	}
}

void FOpenMobileAdsAdMobAndroidBackend::Shutdown()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		return;
	}

	static jmethodID ShutdownMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_ShutdownOpenMobileRewardedAds",
		"()V",
		false
	);
	if (ShutdownMethod)
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, ShutdownMethod);
	}
}

bool FOpenMobileAdsAdMobAndroidBackend::LaunchRewardedAd(
	const FString& AdUnitId,
	const int64 RequestId,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID RequestMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_RequestAndShowOpenMobileRewardedAd",
		"(Ljava/lang/String;J)Z",
		false
	);

	if (!RequestMethod)
	{
		OutError = TEXT("The Android rewarded-ad bridge was not packaged into GameActivity.");
		return false;
	}

	const FScopedJavaObject<jstring> JavaAdUnitId = FJavaHelper::ToJavaString(Env, AdUnitId);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		RequestMethod,
		*JavaAdUnitId,
		static_cast<jlong>(RequestId)
	);

	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the rewarded-ad request.");
	}

	return bScheduled;
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdLoaded(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeLoaded(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdShown(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeShown(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdEarned(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jint NetworkAmount,
	jstring NetworkRewardType
)
{
	FOpenMobileAdsAdMobPlatform::NativeEarned(
		static_cast<int64>(RequestId),
		static_cast<int32>(NetworkAmount),
		FJavaHelper::FStringFromParam(Env, NetworkRewardType)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdClosed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeClosed(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorMessage
)
{
	FOpenMobileAdsAdMobPlatform::NativeFailed(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, ErrorMessage)
	);
}

#endif

#include "OpenMobileAdsAdMobAndroidBackend.h"

#include "OpenMobileAdsAdMobPlatform.h"

#if PLATFORM_ANDROID

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

namespace OpenMobileAdsAdMobAndroidBackendPrivate
{
	int32 ToNativeAgeTreatment(EOpenMobileAdsAgeTreatment Treatment)
	{
		switch (Treatment)
		{
		case EOpenMobileAdsAgeTreatment::No:
			return 0;
		case EOpenMobileAdsAgeTreatment::Yes:
			return 1;
		default:
			return -1;
		}
	}

	FString ToNativeMaxAdContentRating(EOpenMobileAdsMaxAdContentRating Rating)
	{
		switch (Rating)
		{
		case EOpenMobileAdsMaxAdContentRating::General:
			return TEXT("G");
		case EOpenMobileAdsMaxAdContentRating::ParentalGuidance:
			return TEXT("PG");
		case EOpenMobileAdsMaxAdContentRating::Teen:
			return TEXT("T");
		case EOpenMobileAdsMaxAdContentRating::Mature:
			return TEXT("MA");
		default:
			return FString();
		}
	}
}

bool FOpenMobileAdsAdMobAndroidBackend::Initialize(
	const FOpenMobileAdsInitializationRequest& Request,
	const int64 RequestId,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable during AdMob initialization.");
		return false;
	}

	static jmethodID InitializeMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_InitializeOpenMobileRewardedAds",
		"(JIIZLjava/lang/String;)Z",
		false
	);

	if (!InitializeMethod)
	{
		OutError = TEXT("The Android AdMob initialization bridge was not packaged into GameActivity.");
		return false;
	}

	const FString Rating = OpenMobileAdsAdMobAndroidBackendPrivate::ToNativeMaxAdContentRating(
		Request.RequestConfiguration.MaxAdContentRating
	);
	const FScopedJavaObject<jstring> JavaRating = FJavaHelper::ToJavaString(Env, Rating);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		InitializeMethod,
		static_cast<jlong>(RequestId),
		static_cast<jint>(OpenMobileAdsAdMobAndroidBackendPrivate::ToNativeAgeTreatment(
			Request.Privacy.ChildDirectedTreatment
		)),
		static_cast<jint>(OpenMobileAdsAdMobAndroidBackendPrivate::ToNativeAgeTreatment(
			Request.Privacy.UnderAgeOfConsent
		)),
		static_cast<jboolean>(Request.bDevelopmentTestMode),
		*JavaRating
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule AdMob SDK initialization.");
	}
	return bScheduled;
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

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileAdsInitializationCompleted(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileAdsInitializationFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorMessage
)
{
	FOpenMobileAdsAdMobPlatform::NativeInitializationFailed(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, ErrorMessage)
	);
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

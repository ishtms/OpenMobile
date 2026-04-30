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

	int32 ToBridgeDebugGeography(EOpenMobileAdsDebugGeography Geography)
	{
		switch (Geography)
		{
		case EOpenMobileAdsDebugGeography::Eea:
			return 1;
		case EOpenMobileAdsDebugGeography::RegulatedUsState:
			return 2;
		case EOpenMobileAdsDebugGeography::Other:
			return 3;
		default:
			return 0;
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
		"(JII[Ljava/lang/String;Ljava/lang/String;)Z",
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
	TArray<FStringView> TestDeviceIdentifierViews;
	TestDeviceIdentifierViews.Reserve(Request.Development.TestDeviceIdentifiers.Num());
	for (const FString& Identifier : Request.Development.TestDeviceIdentifiers)
	{
		TestDeviceIdentifierViews.Add(Identifier);
	}
	const FScopedJavaObject<jobjectArray> JavaTestDeviceIdentifiers =
		FJavaHelper::ToJavaStringArray(Env, TestDeviceIdentifierViews);
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
		*JavaTestDeviceIdentifiers,
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

bool FOpenMobileAdsAdMobAndroidBackend::RequestConsentInfo(
	const FOpenMobileAdsConsentRequest& Request,
	const int64 RequestId,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable during the UMP request.");
		return false;
	}
	static jmethodID RequestMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_RequestOpenMobileUMPConsent",
		"(JZ[Ljava/lang/String;I)Z",
		false
	);
	if (!RequestMethod)
	{
		OutError = TEXT("The Android Google UMP request bridge was not packaged into GameActivity.");
		return false;
	}

	TArray<FStringView> TestDeviceIdentifierViews;
	if (
		Request.Development.bEnableConsentDebug
		&& Request.Privacy.UnderAgeOfConsent
			!= EOpenMobileAdsAgeTreatment::Yes
	)
	{
		TestDeviceIdentifierViews.Reserve(
			Request.Development.TestDeviceIdentifiers.Num()
		);
		for (const FString& Identifier : Request.Development.TestDeviceIdentifiers)
		{
			TestDeviceIdentifierViews.Add(Identifier);
		}
	}
	const FScopedJavaObject<jobjectArray> JavaTestDeviceIdentifiers =
		FJavaHelper::ToJavaStringArray(Env, TestDeviceIdentifierViews);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		RequestMethod,
		static_cast<jlong>(RequestId),
		static_cast<jboolean>(
			Request.Privacy.UnderAgeOfConsent
				== EOpenMobileAdsAgeTreatment::Yes
		),
		*JavaTestDeviceIdentifiers,
		static_cast<jint>(
			OpenMobileAdsAdMobAndroidBackendPrivate::ToBridgeDebugGeography(
				Request.Development.GetEffectiveDebugGeography()
			)
		)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the Google UMP consent-info update.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::PresentRequiredConsentForm(
	const int64 RequestId,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable during UMP form presentation.");
		return false;
	}
	static jmethodID PresentMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_PresentRequiredOpenMobileUMPConsentForm",
		"(J)Z",
		false
	);
	if (!PresentMethod)
	{
		OutError = TEXT("The Android Google UMP form bridge was not packaged into GameActivity.");
		return false;
	}
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		PresentMethod,
		static_cast<jlong>(RequestId)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the required Google UMP form.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::PresentPrivacyOptionsForm(
	const int64 RequestId,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable during UMP privacy-options presentation.");
		return false;
	}
	static jmethodID PresentMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_PresentOpenMobileUMPPrivacyOptionsForm",
		"(J)Z",
		false
	);
	if (!PresentMethod)
	{
		OutError = TEXT("The Android Google UMP privacy-options bridge was not packaged into GameActivity.");
		return false;
	}
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		PresentMethod,
		static_cast<jlong>(RequestId)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the Google UMP privacy-options form.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::ResetConsentForTesting(
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable during UMP consent reset.");
		return false;
	}
	static jmethodID ResetMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_ResetOpenMobileUMPConsent",
		"()Z",
		false
	);
	if (!ResetMethod)
	{
		OutError = TEXT("The Android Google UMP reset bridge was not packaged into GameActivity.");
		return false;
	}
	const bool bReset = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		ResetMethod
	);
	if (!bReset)
	{
		OutError = TEXT("Android could not reset Google UMP consent state.");
	}
	return bReset;
}

bool FOpenMobileAdsAdMobAndroidBackend::ApplyConsentSignals(
	const FOpenMobileAdsConsentSignals& Signals,
	int32 SignalMask,
	FString& OutError
)
{
	if (
		(SignalMask & static_cast<int32>(
			EOpenMobileAdsConsentSignal::UsPrivacy
		)) == 0
	)
	{
		return true;
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable while applying consent signals.");
		return false;
	}
	static jmethodID ApplyMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_ApplyOpenMobileAdsConsentSignals",
		"(I)Z",
		false
	);
	if (!ApplyMethod)
	{
		OutError = TEXT("The Android AdMob consent-signal bridge was not packaged into GameActivity.");
		return false;
	}
	const bool bApplied = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		ApplyMethod,
		static_cast<jint>(Signals.UsPrivacy.DataProcessingMode)
	);
	if (!bApplied)
	{
		OutError = TEXT("Android could not apply AdMob consent signals.");
	}
	return bApplied;
}

bool FOpenMobileAdsAdMobAndroidBackend::LoadRewardedAd(
	const FString& AdUnitId,
	const int64 RequestId,
	EOpenMobileAdsDataProcessingMode DataProcessingMode,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID LoadMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_LoadOpenMobileRewardedAd",
		"(Ljava/lang/String;JI)Z",
		false
	);
	if (!LoadMethod)
	{
		OutError = TEXT("The Android rewarded-ad load bridge was not packaged into GameActivity.");
		return false;
	}

	const FScopedJavaObject<jstring> JavaAdUnitId = FJavaHelper::ToJavaString(Env, AdUnitId);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		LoadMethod,
		*JavaAdUnitId,
		static_cast<jlong>(RequestId),
		static_cast<jint>(DataProcessingMode)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the rewarded-ad load.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::LoadInterstitialAd(
	const FString& AdUnitId,
	const int64 RequestId,
	EOpenMobileAdsDataProcessingMode DataProcessingMode,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID LoadMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_LoadOpenMobileInterstitialAd",
		"(Ljava/lang/String;JI)Z",
		false
	);
	if (!LoadMethod)
	{
		OutError = TEXT("The Android interstitial load bridge was not packaged into GameActivity.");
		return false;
	}

	const FScopedJavaObject<jstring> JavaAdUnitId = FJavaHelper::ToJavaString(Env, AdUnitId);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		LoadMethod,
		*JavaAdUnitId,
		static_cast<jlong>(RequestId),
		static_cast<jint>(DataProcessingMode)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the interstitial load.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::LoadRewardedInterstitialAd(
	const FString& AdUnitId,
	const int64 RequestId,
	EOpenMobileAdsDataProcessingMode DataProcessingMode,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID LoadMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_LoadOpenMobileRewardedInterstitialAd",
		"(Ljava/lang/String;JI)Z",
		false
	);
	if (!LoadMethod)
	{
		OutError = TEXT("The Android rewarded-interstitial load bridge was not packaged into GameActivity.");
		return false;
	}

	const FScopedJavaObject<jstring> JavaAdUnitId =
		FJavaHelper::ToJavaString(Env, AdUnitId);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		LoadMethod,
		*JavaAdUnitId,
		static_cast<jlong>(RequestId),
		static_cast<jint>(DataProcessingMode)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the rewarded-interstitial load.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::LoadBannerAd(
	const FString& AdUnitId,
	const int64 RequestId,
	EOpenMobileAdsDataProcessingMode DataProcessingMode,
	EOpenMobileAdFormat Format,
	const FOpenMobileAdsBannerLayout& Layout,
	FString& OutError
)
{
	int32 NativeFormat = 0;
	if (Format == EOpenMobileAdFormat::AnchoredAdaptiveBanner)
	{
		NativeFormat = 1;
	}
	else if (Format == EOpenMobileAdFormat::MediumRectangle)
	{
		NativeFormat = 2;
	}
	else if (Format != EOpenMobileAdFormat::Banner)
	{
		OutError = TEXT("Android received an unsupported persistent ad format.");
		return false;
	}

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID LoadMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_LoadOpenMobileBannerAd",
		"(Ljava/lang/String;JIIIIZFFFFF)Z",
		false
	);
	if (!LoadMethod)
	{
		OutError = TEXT("The Android banner load bridge was not packaged into GameActivity.");
		return false;
	}

	const FScopedJavaObject<jstring> JavaAdUnitId = FJavaHelper::ToJavaString(
		Env,
		AdUnitId
	);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		LoadMethod,
		*JavaAdUnitId,
		static_cast<jlong>(RequestId),
		static_cast<jint>(DataProcessingMode),
		static_cast<jint>(NativeFormat),
		static_cast<jint>(Layout.Anchor),
		static_cast<jint>(Layout.HorizontalAlignment),
		static_cast<jboolean>(Layout.bRespectSafeArea),
		static_cast<jfloat>(Layout.AvailableWidth),
		static_cast<jfloat>(Layout.Margins.Left),
		static_cast<jfloat>(Layout.Margins.Top),
		static_cast<jfloat>(Layout.Margins.Right),
		static_cast<jfloat>(Layout.Margins.Bottom)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the banner load.");
	}
	return bScheduled;
}

void FOpenMobileAdsAdMobAndroidBackend::CancelRewardedAd(const int64 RequestId)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		return;
	}

	static jmethodID CancelMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_CancelOpenMobileRewardedAdLoad",
		"(J)V",
		false
	);
	if (CancelMethod)
	{
		FJavaWrapper::CallVoidMethod(
			Env,
			FJavaWrapper::GameActivityThis,
			CancelMethod,
			static_cast<jlong>(RequestId)
		);
	}
}

void FOpenMobileAdsAdMobAndroidBackend::CancelInterstitialAd(
	const int64 RequestId
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		return;
	}

	static jmethodID CancelMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_CancelOpenMobileInterstitialAdLoad",
		"(J)V",
		false
	);
	if (CancelMethod)
	{
		FJavaWrapper::CallVoidMethod(
			Env,
			FJavaWrapper::GameActivityThis,
			CancelMethod,
			static_cast<jlong>(RequestId)
		);
	}
}

void FOpenMobileAdsAdMobAndroidBackend::CancelRewardedInterstitialAd(
	const int64 RequestId
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		return;
	}

	static jmethodID CancelMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_CancelOpenMobileRewardedInterstitialAdLoad",
		"(J)V",
		false
	);
	if (CancelMethod)
	{
		FJavaWrapper::CallVoidMethod(
			Env,
			FJavaWrapper::GameActivityThis,
			CancelMethod,
			static_cast<jlong>(RequestId)
		);
	}
}

void FOpenMobileAdsAdMobAndroidBackend::CancelBannerAd(const int64 RequestId)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		return;
	}

	static jmethodID CancelMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_CancelOpenMobileBannerAd",
		"(J)V",
		false
	);
	if (CancelMethod)
	{
		FJavaWrapper::CallVoidMethod(
			Env,
			FJavaWrapper::GameActivityThis,
			CancelMethod,
			static_cast<jlong>(RequestId)
		);
	}
}

bool FOpenMobileAdsAdMobAndroidBackend::ShowRewardedAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	const FString& ServerVerificationCustomData,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID ShowMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_ShowOpenMobileRewardedAd",
		"(JJLjava/lang/String;)Z",
		false
	);
	if (!ShowMethod)
	{
		OutError = TEXT("The Android rewarded-ad show bridge was not packaged into GameActivity.");
		return false;
	}

	const FScopedJavaObject<jstring> JavaCustomData = FJavaHelper::ToJavaString(
		Env,
		ServerVerificationCustomData
	);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		ShowMethod,
		static_cast<jlong>(LoadedRequestId),
		static_cast<jlong>(ShowRequestId),
		*JavaCustomData
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the cached rewarded-ad presentation.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::ShowInterstitialAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID ShowMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_ShowOpenMobileInterstitialAd",
		"(JJ)Z",
		false
	);
	if (!ShowMethod)
	{
		OutError = TEXT("The Android interstitial show bridge was not packaged into GameActivity.");
		return false;
	}

	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		ShowMethod,
		static_cast<jlong>(LoadedRequestId),
		static_cast<jlong>(ShowRequestId)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the cached interstitial presentation.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::ShowRewardedInterstitialAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	const FString& ServerVerificationCustomData,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID ShowMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_ShowOpenMobileRewardedInterstitialAd",
		"(JJLjava/lang/String;)Z",
		false
	);
	if (!ShowMethod)
	{
		OutError = TEXT("The Android rewarded-interstitial show bridge was not packaged into GameActivity.");
		return false;
	}

	const FScopedJavaObject<jstring> JavaCustomData = FJavaHelper::ToJavaString(
		Env,
		ServerVerificationCustomData
	);
	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		ShowMethod,
		static_cast<jlong>(LoadedRequestId),
		static_cast<jlong>(ShowRequestId),
		*JavaCustomData
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the cached rewarded-interstitial presentation.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::ShowBannerAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	const FOpenMobileAdsBannerLayout& Layout,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID ShowMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_ShowOpenMobileBannerAd",
		"(JJIIZFFFFF)Z",
		false
	);
	if (!ShowMethod)
	{
		OutError = TEXT("The Android banner show bridge was not packaged into GameActivity.");
		return false;
	}

	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		ShowMethod,
		static_cast<jlong>(LoadedRequestId),
		static_cast<jlong>(ShowRequestId),
		static_cast<jint>(Layout.Anchor),
		static_cast<jint>(Layout.HorizontalAlignment),
		static_cast<jboolean>(Layout.bRespectSafeArea),
		static_cast<jfloat>(Layout.AvailableWidth),
		static_cast<jfloat>(Layout.Margins.Left),
		static_cast<jfloat>(Layout.Margins.Top),
		static_cast<jfloat>(Layout.Margins.Right),
		static_cast<jfloat>(Layout.Margins.Bottom)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the cached banner presentation.");
	}
	return bScheduled;
}

bool FOpenMobileAdsAdMobAndroidBackend::HideBannerAd(
	const int64 LoadedRequestId,
	const int64 HideRequestId,
	FString& OutError
)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		OutError = TEXT("Android's Java environment is unavailable.");
		return false;
	}

	static jmethodID HideMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_HideOpenMobileBannerAd",
		"(JJ)Z",
		false
	);
	if (!HideMethod)
	{
		OutError = TEXT("The Android banner hide bridge was not packaged into GameActivity.");
		return false;
	}

	const bool bScheduled = FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		HideMethod,
		static_cast<jlong>(LoadedRequestId),
		static_cast<jlong>(HideRequestId)
	);
	if (!bScheduled)
	{
		OutError = TEXT("Android could not schedule the cached banner hide.");
	}
	return bScheduled;
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

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdLoadCompleted(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeRewardedLoadCompleted(
		static_cast<int64>(RequestId)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdLoadFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorMessage
)
{
	FOpenMobileAdsAdMobPlatform::NativeRewardedLoadFailed(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, ErrorMessage)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedInterstitialAdLoadCompleted(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jint RewardAmount,
	jstring RewardType
)
{
	FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadCompleted(
		static_cast<int64>(RequestId),
		static_cast<int64>(RewardAmount),
		RewardType ? FJavaHelper::FStringFromParam(Env, RewardType) : FString()
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedInterstitialAdLoadFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorMessage
)
{
	FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadFailed(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, ErrorMessage)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileInterstitialAdLoadCompleted(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadCompleted(
		static_cast<int64>(RequestId)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileInterstitialAdLoadFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorMessage
)
{
	FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadFailed(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, ErrorMessage)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileBannerAdLoadCompleted(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeBannerLoadCompleted(
		static_cast<int64>(RequestId)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileBannerAdLoadFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorMessage
)
{
	FOpenMobileAdsAdMobPlatform::NativeBannerLoadFailed(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, ErrorMessage)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileBannerAdShown(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeBannerShown(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileBannerAdHidden(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeBannerHidden(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileBannerAdOperationFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorMessage
)
{
	FOpenMobileAdsAdMobPlatform::NativeBannerOperationFailed(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, ErrorMessage)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileBannerAdImpression(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeImpression(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileBannerAdClicked(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeClicked(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileBannerAdRevenuePaid(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jlong ValueMicros,
	jstring CurrencyCode,
	jint Precision
)
{
	FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
		static_cast<int64>(RequestId),
		static_cast<int64>(ValueMicros),
		CurrencyCode ? FJavaHelper::FStringFromParam(Env, CurrencyCode) : FString(),
		static_cast<int32>(Precision)
	);
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

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileUMPConsentInfoUpdated(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jint ConsentStatus,
	jboolean bCanRequestAds,
	jint PrivacyOptionsRequirement
)
{
	FOpenMobileAdsAdMobPlatform::NativeConsentInfoUpdated(
		static_cast<int64>(RequestId),
		static_cast<int32>(ConsentStatus),
		static_cast<bool>(bCanRequestAds),
		static_cast<int32>(PrivacyOptionsRequirement)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileUMPConsentFormDismissed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jint ConsentStatus,
	jboolean bCanRequestAds,
	jint PrivacyOptionsRequirement
)
{
	FOpenMobileAdsAdMobPlatform::NativeConsentFormDismissed(
		static_cast<int64>(RequestId),
		static_cast<int32>(ConsentStatus),
		static_cast<bool>(bCanRequestAds),
		static_cast<int32>(PrivacyOptionsRequirement)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileUMPConsentFailed(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring ErrorCode,
	jstring ErrorMessage
)
{
	FOpenMobileAdsAdMobPlatform::NativeConsentFailed(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, ErrorCode),
		FJavaHelper::FStringFromParam(Env, ErrorMessage)
	);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileAdsAdapterInitializationStatus(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jstring AdapterName,
	jboolean bReady,
	jlong LatencyMilliseconds,
	jstring Description
)
{
	FOpenMobileAdsAdMobPlatform::NativeAdapterInitializationStatus(
		static_cast<int64>(RequestId),
		FJavaHelper::FStringFromParam(Env, AdapterName),
		static_cast<bool>(bReady),
		static_cast<double>(LatencyMilliseconds),
		FJavaHelper::FStringFromParam(Env, Description)
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

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdImpression(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeImpression(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdClicked(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId
)
{
	FOpenMobileAdsAdMobPlatform::NativeClicked(static_cast<int64>(RequestId));
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileRewardedAdRevenuePaid(
	JNIEnv* Env,
	jobject Activity,
	jlong RequestId,
	jlong ValueMicros,
	jstring CurrencyCode,
	jint Precision
)
{
	FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
		static_cast<int64>(RequestId),
		static_cast<int64>(ValueMicros),
		CurrencyCode ? FJavaHelper::FStringFromParam(Env, CurrencyCode) : FString(),
		static_cast<int32>(Precision)
	);
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

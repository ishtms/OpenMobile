#include "OpenMobileDeviceAndroidAccessibility.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceAssistiveTechnology.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDevicePreferredTextScale.h"
#include "OpenMobileDeviceReducedAnimation.h"

namespace OpenMobileDeviceAndroidAccessibilityPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;

	void ClearState()
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		SourceSequence = 0;
	}

	void NotifyChange()
	{
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 Sequence = 0;
		{
			FScopeLock Lock(&StateMutex);
			if (!ActiveToken.IsValid())
			{
				return;
			}
			SourceSequence = SourceSequence == MAX_uint64
				? 1
				: SourceSequence + 1;
			CallbackToken = ActiveToken;
			Sequence = SourceSequence;
		}
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(
			CallbackToken,
			Sequence
		);
	}
}

FOpenMobileAccessibilitySnapshot
GetOpenMobileDeviceAndroidAccessibilitySnapshot()
{
	check(IsInGameThread());
	FOpenMobileAccessibilitySnapshot Snapshot;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !FJavaWrapper::GameActivityThis)
	{
		return Snapshot;
	}
	static jmethodID GetFontScaleMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceGetPreferredTextScale",
		"()F",
		false
	);
	if (GetFontScaleMethod)
	{
		const jfloat FontScale = FJavaWrapper::CallFloatMethod(
			Env,
			FJavaWrapper::GameActivityThis,
			GetFontScaleMethod
		);
		if (!Env->ExceptionCheck())
		{
			const FOpenMobileAccessibilitySnapshot TextScale =
				FOpenMobileDevicePreferredTextScale::FromAndroidFontScale(
					FontScale
				);
			Snapshot.PreferredTextScale = TextScale.PreferredTextScale;
		}
		else
		{
			Env->ExceptionClear();
		}
	}

	static jmethodID GetAnimationScalesMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceGetAnimationScales",
		"()[F",
		false
	);
	if (GetAnimationScalesMethod)
	{
		FScopedJavaObject<jfloatArray> AnimationScales(
			static_cast<jfloatArray>(FJavaWrapper::CallObjectMethod(
				Env,
				FJavaWrapper::GameActivityThis,
				GetAnimationScalesMethod
			))
		);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
		}
		else if (AnimationScales
			&& Env->GetArrayLength(*AnimationScales) == 3)
		{
			jfloat Values[3] = {};
			Env->GetFloatArrayRegion(*AnimationScales, 0, 3, Values);
			if (!Env->ExceptionCheck())
			{
				const FOpenMobileAccessibilitySnapshot ReducedAnimation =
					FOpenMobileDeviceReducedAnimation::
						FromAndroidAnimationScales(
							Values[0],
							Values[1],
							Values[2]
						);
				Snapshot.bReducedAnimationPreferred =
					ReducedAnimation.bReducedAnimationPreferred;
				Snapshot.ReducedAnimationPlatformDetail =
					ReducedAnimation.ReducedAnimationPlatformDetail;
			}
			else
			{
				Env->ExceptionClear();
			}
		}
	}

	static jmethodID GetTouchExplorationMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceGetTouchExplorationState",
		"()I",
		false
	);
	if (GetTouchExplorationMethod)
	{
		const jint TouchExplorationState = FJavaWrapper::CallIntMethod(
			Env,
			FJavaWrapper::GameActivityThis,
			GetTouchExplorationMethod
		);
		if (!Env->ExceptionCheck())
		{
			const FOpenMobileAccessibilitySnapshot AssistiveTechnology =
				FOpenMobileDeviceAssistiveTechnology::
					FromAndroidTouchExplorationState(
						TouchExplorationState
					);
			Snapshot.bTouchExplorationActive =
				AssistiveTechnology.bTouchExplorationActive;
		}
		else
		{
			Env->ExceptionClear();
		}
	}
	return Snapshot;
}

bool StartOpenMobileDeviceAndroidAccessibilityMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidAccessibilityPrivate;
	if (!CallbackToken.IsValid())
	{
		return false;
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !FJavaWrapper::GameActivityThis)
	{
		return false;
	}
	static jmethodID StartMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceStartAccessibilityMonitoring",
		"()Z",
		false
	);
	if (!StartMethod)
	{
		return false;
	}
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = CallbackToken;
		SourceSequence = 0;
	}
	if (!FJavaWrapper::CallBooleanMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		StartMethod
	))
	{
		ClearState();
		return false;
	}
	return true;
}

void StopOpenMobileDeviceAndroidAccessibilityMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidAccessibilityPrivate;
	ClearState();
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !FJavaWrapper::GameActivityThis)
	{
		return;
	}
	static jmethodID StopMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceStopAccessibilityMonitoring",
		"()V",
		false
	);
	if (StopMethod)
	{
		FJavaWrapper::CallVoidMethod(
			Env,
			FJavaWrapper::GameActivityThis,
			StopMethod
		);
	}
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileDeviceAccessibilityChanged(
	JNIEnv* Env,
	jobject Activity
)
{
	static_cast<void>(Env);
	static_cast<void>(Activity);
	OpenMobileDeviceAndroidAccessibilityPrivate::NotifyChange();
}

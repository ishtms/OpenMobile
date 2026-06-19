#include "OpenMobileDeviceAndroidAppearance.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceSystemAppearance.h"

namespace OpenMobileDeviceAndroidAppearancePrivate
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

FOpenMobileAppearanceSnapshot GetOpenMobileDeviceAndroidAppearanceSnapshot()
{
	check(IsInGameThread());
	FOpenMobileAppearanceSnapshot Snapshot;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !FJavaWrapper::GameActivityThis)
	{
		return Snapshot;
	}
	static jmethodID GetMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceGetSystemAppearance",
		"()I",
		false
	);
	if (!GetMethod)
	{
		return Snapshot;
	}
	const jint NightMode = FJavaWrapper::CallIntMethod(
		Env,
		FJavaWrapper::GameActivityThis,
		GetMethod
	);
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		return Snapshot;
	}
	Snapshot.Appearance =
		FOpenMobileDeviceSystemAppearance::FromAndroidNightMode(NightMode);
	return Snapshot;
}

bool StartOpenMobileDeviceAndroidAppearanceMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidAppearancePrivate;
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
		"AndroidThunkJava_OpenMobileDeviceStartAppearanceMonitoring",
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

void StopOpenMobileDeviceAndroidAppearanceMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidAppearancePrivate;
	ClearState();
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !FJavaWrapper::GameActivityThis)
	{
		return;
	}
	static jmethodID StopMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceStopAppearanceMonitoring",
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

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileDeviceAppearanceChanged(
	JNIEnv* Env,
	jobject Activity
)
{
	static_cast<void>(Env);
	static_cast<void>(Activity);
	OpenMobileDeviceAndroidAppearancePrivate::NotifyChange();
}

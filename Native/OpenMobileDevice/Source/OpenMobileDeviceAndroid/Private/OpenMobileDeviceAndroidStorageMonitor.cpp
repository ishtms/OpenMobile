#include "OpenMobileDeviceAndroidStorageMonitor.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMonitoringService.h"

namespace OpenMobileDeviceAndroidStorageMonitorPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;

	void RecordChange()
	{
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 CallbackSequence = 0;
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
			CallbackSequence = SourceSequence;
		}
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(
			CallbackToken,
			CallbackSequence
		);
	}

	void ClearState()
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		SourceSequence = 0;
	}
}

bool StartOpenMobileDeviceAndroidStorageMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidStorageMonitorPrivate;
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
		"AndroidThunkJava_OpenMobileDeviceStartStorageMonitoring",
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

void StopOpenMobileDeviceAndroidStorageMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidStorageMonitorPrivate;
	ClearState();
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !FJavaWrapper::GameActivityThis)
	{
		return;
	}
	static jmethodID StopMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceStopStorageMonitoring",
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

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileDeviceStorageChanged(
	JNIEnv* Env,
	jobject Activity
)
{
	static_cast<void>(Env);
	static_cast<void>(Activity);
	OpenMobileDeviceAndroidStorageMonitorPrivate::RecordChange();
}

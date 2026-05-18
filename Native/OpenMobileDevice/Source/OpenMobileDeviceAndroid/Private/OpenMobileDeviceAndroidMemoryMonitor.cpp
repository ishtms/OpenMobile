#include "OpenMobileDeviceAndroidMemoryMonitor.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMemoryPressureInfo.h"
#include "OpenMobileDeviceMonitoringService.h"

namespace OpenMobileDeviceAndroidMemoryMonitorPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;
	int64 EventSequence = 0;
	EOpenMobileMemoryPressureState LatestEventState =
		EOpenMobileMemoryPressureState::Unknown;
	FOpenMobileDeviceOptionalInt32 NativeLevel;
	FDateTime EventTimeUtc;

	void RecordEvent(int32 InNativeLevel)
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
			EventSequence = EventSequence == MAX_int64
				? 1
				: EventSequence + 1;
			LatestEventState = InNativeLevel < 0
				? EOpenMobileMemoryPressureState::Critical
				: FOpenMobileDeviceMemoryPressureInfo::NormalizeAndroidTrimLevel(
					InNativeLevel
				);
			NativeLevel = InNativeLevel >= 0
				? FOpenMobileDeviceOptionalInt32::MakeAvailable(InNativeLevel)
				: FOpenMobileDeviceOptionalInt32();
			EventTimeUtc = FDateTime::UtcNow();
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
		EventSequence = 0;
		LatestEventState = EOpenMobileMemoryPressureState::Unknown;
		NativeLevel = {};
		EventTimeUtc = {};
	}
}

void ApplyOpenMobileDeviceAndroidMemoryPressureEvent(
	FOpenMobileMemorySnapshot& Snapshot
)
{
	using namespace OpenMobileDeviceAndroidMemoryMonitorPrivate;
	FScopeLock Lock(&StateMutex);
	Snapshot.LatestPressureEventState = LatestEventState;
	Snapshot.NativeMemoryPressureLevel = NativeLevel;
	Snapshot.PressureEventTimeUtc = EventTimeUtc;
	Snapshot.PressureEventSequence = EventSequence;
}

bool StartOpenMobileDeviceAndroidMemoryMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidMemoryMonitorPrivate;
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
		"AndroidThunkJava_OpenMobileDeviceStartMemoryMonitoring",
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
		EventSequence = 0;
		LatestEventState = EOpenMobileMemoryPressureState::Unknown;
		NativeLevel = {};
		EventTimeUtc = {};
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

void StopOpenMobileDeviceAndroidMemoryMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidMemoryMonitorPrivate;
	ClearState();
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !FJavaWrapper::GameActivityThis)
	{
		return;
	}
	static jmethodID StopMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceStopMemoryMonitoring",
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

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileDeviceMemoryPressure(
	JNIEnv* Env,
	jobject Activity,
	jint NativeLevel
)
{
	static_cast<void>(Env);
	static_cast<void>(Activity);
	OpenMobileDeviceAndroidMemoryMonitorPrivate::RecordEvent(
		static_cast<int32>(NativeLevel)
	);
}

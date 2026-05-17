#include "OpenMobileDeviceAndroidBattery.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceBatteryInfo.h"
#include "OpenMobileDeviceMonitoringService.h"

namespace OpenMobileDeviceAndroidBatteryPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;

	bool CallStringArrayMethod(
		const char* MethodName,
		TArray<FString>& OutValues
	)
	{
		OutValues.Reset();
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Env || !Activity)
		{
			return false;
		}
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID Method = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				MethodName,
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
			return false;
		}

		FScopedJavaObject<jobjectArray> Values(static_cast<jobjectArray>(
			Env->CallObjectMethod(Activity, Method)
		));
		const bool bArrayError = Env->ExceptionCheck();
		if (bArrayError)
		{
			Env->ExceptionClear();
		}
		if (!Values || bArrayError)
		{
			return false;
		}
		const jsize Count = Env->GetArrayLength(*Values);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return false;
		}
		OutValues.Reserve(Count);
		for (jsize Index = 0; Index < Count; ++Index)
		{
			jstring Value = static_cast<jstring>(
				Env->GetObjectArrayElement(*Values, Index)
			);
			if (Env->ExceptionCheck())
			{
				Env->ExceptionClear();
				OutValues.Reset();
				return false;
			}
			OutValues.Add(FJavaHelper::FStringFromLocalRef(Env, Value));
		}
		return true;
	}

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

FOpenMobilePowerSnapshot GetOpenMobileDeviceAndroidPowerSnapshot()
{
	using namespace OpenMobileDeviceAndroidBatteryPrivate;
	FOpenMobilePowerSnapshot Snapshot;
	TArray<FString> Details;
	if (CallStringArrayMethod(
		"AndroidThunkJava_OpenMobileDeviceGetBatteryDetails",
		Details
	) && Details.Num() == 7)
	{
		const bool bLevelValid = Details[0].IsNumeric();
		const bool bScaleValid = Details[1].IsNumeric();
		const bool bBatteryPresent = Details[2].Equals(
			TEXT("true"),
			ESearchCase::IgnoreCase
		);
		FOpenMobileDeviceBatteryInfo::ApplyRatio(
			Snapshot,
			bLevelValid ? FCString::Atoi64(*Details[0]) : 0,
			bScaleValid ? FCString::Atoi64(*Details[1]) : 0,
			bLevelValid && bScaleValid && bBatteryPresent
		);
		const bool bStateValid = Details[3].IsNumeric();
		const int64 NativeState = bStateValid
			? FCString::Atoi64(*Details[3])
			: 0;
		FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
			Snapshot,
			NativeState,
			bStateValid && NativeState >= 0
		);
		const bool bSourceValid = Details[4].IsNumeric();
		FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingSource(
			Snapshot,
			bSourceValid ? FCString::Atoi64(*Details[4]) : 0,
			bSourceValid
		);
		const bool bPowerSavingAvailable = Details[5].Equals(
			TEXT("true"),
			ESearchCase::IgnoreCase
		) || Details[5].Equals(TEXT("false"), ESearchCase::IgnoreCase);
		FOpenMobileDeviceBatteryInfo::ApplyAndroidPowerSavingState(
			Snapshot,
			Details[5].Equals(TEXT("true"), ESearchCase::IgnoreCase),
			bPowerSavingAvailable
		);
		const bool bThermalAvailable = Details[6].IsNumeric();
		FOpenMobileDeviceBatteryInfo::ApplyAndroidThermalState(
			Snapshot,
			bThermalAvailable ? FCString::Atoi(*Details[6]) : 0,
			bThermalAvailable
		);
	}
	return Snapshot;
}

bool StartOpenMobileDeviceAndroidBatteryMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidBatteryPrivate;
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
		"AndroidThunkJava_OpenMobileDeviceStartBatteryMonitoring",
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

void StopOpenMobileDeviceAndroidBatteryMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidBatteryPrivate;
	ClearState();
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !FJavaWrapper::GameActivityThis)
	{
		return;
	}
	static jmethodID StopMethod = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_OpenMobileDeviceStopBatteryMonitoring",
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

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOpenMobileDeviceBatteryChanged(
	JNIEnv* Env,
	jobject Activity
)
{
	static_cast<void>(Env);
	static_cast<void>(Activity);
	OpenMobileDeviceAndroidBatteryPrivate::NotifyChange();
}

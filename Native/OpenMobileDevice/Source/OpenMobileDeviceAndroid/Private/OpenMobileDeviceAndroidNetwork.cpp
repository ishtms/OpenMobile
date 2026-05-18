#include "OpenMobileDeviceAndroidNetwork.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OpenMobileDeviceNetworkPathInfo.h"

namespace OpenMobileDeviceAndroidNetworkPrivate
{
	bool ParseBoolean(const FString& Value, bool& bOutValue)
	{
		if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			bOutValue = true;
			return true;
		}
		if (Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			bOutValue = false;
			return true;
		}
		return false;
	}
}

FOpenMobileNetworkPathSnapshot GetOpenMobileDeviceAndroidNetworkPathSnapshot()
{
	FOpenMobileDeviceAndroidNetworkPathTraits Traits;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		return FOpenMobileDeviceNetworkPathInfo::BuildAndroid(Traits);
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceGetNetworkPath",
			"()[Ljava/lang/String;"
		)
		: nullptr;
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		return FOpenMobileDeviceNetworkPathInfo::BuildAndroid(Traits);
	}
	if (!Method)
	{
		return FOpenMobileDeviceNetworkPathInfo::BuildAndroid(Traits);
	}
	FScopedJavaObject<jobjectArray> Result(static_cast<jobjectArray>(
		Env->CallObjectMethod(Activity, Method)
	));
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		return FOpenMobileDeviceNetworkPathInfo::BuildAndroid(Traits);
	}
	if (!Result || Env->GetArrayLength(*Result) != 8)
	{
		return FOpenMobileDeviceNetworkPathInfo::BuildAndroid(Traits);
	}
	TArray<FString> Values;
	Values.Reserve(8);
	for (jsize Index = 0; Index < 8; ++Index)
	{
		jstring Value = static_cast<jstring>(
			Env->GetObjectArrayElement(*Result, Index)
		);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return FOpenMobileDeviceNetworkPathInfo::BuildAndroid(Traits);
		}
		Values.Add(FJavaHelper::FStringFromLocalRef(Env, Value));
	}
	using namespace OpenMobileDeviceAndroidNetworkPrivate;
	bool ParsedValues[8] = {};
	for (int32 Index = 0; Index < 8; ++Index)
	{
		if (!ParseBoolean(Values[Index], ParsedValues[Index]))
		{
			return FOpenMobileDeviceNetworkPathInfo::BuildAndroid(Traits);
		}
	}
	Traits.bQuerySucceeded = true;
	Traits.bHasActiveNetwork = ParsedValues[0];
	Traits.bCapabilitiesAvailable = ParsedValues[1];
	Traits.bInternetDeclared = ParsedValues[2];
	Traits.bInternetValidated = ParsedValues[3];
	Traits.bCaptivePortalSupported = ParsedValues[4];
	Traits.bCaptivePortal = ParsedValues[5];
	Traits.bLocalNetwork = ParsedValues[6];
	Traits.bRestricted = ParsedValues[7];
	return FOpenMobileDeviceNetworkPathInfo::BuildAndroid(Traits);
}

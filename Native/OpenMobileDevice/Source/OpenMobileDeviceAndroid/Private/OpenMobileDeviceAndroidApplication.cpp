#include "OpenMobileDeviceAndroidApplication.h"

#include "Android/AndroidApplication.h"
#include "Misc/App.h"
#include "OpenMobileDeviceApplicationInfo.h"

namespace OpenMobileDeviceAndroidApplicationPrivate
{
	FString CallStringMethod(const char* MethodName)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Env || !Activity)
		{
			return {};
		}

		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID Method = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				MethodName,
				"()Ljava/lang/String;"
			)
			: nullptr;
		const bool bMethodError = Env->ExceptionCheck();
		if (bMethodError)
		{
			Env->ExceptionClear();
		}
		if (!Method || bMethodError)
		{
			return {};
		}

		jstring Value = static_cast<jstring>(
			Env->CallObjectMethod(Activity, Method)
		);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return {};
		}
		return FJavaHelper::FStringFromLocalRef(Env, Value);
	}
}

FOpenMobileApplicationMetadataSnapshot
GetOpenMobileDeviceAndroidApplicationMetadata()
{
	using namespace OpenMobileDeviceAndroidApplicationPrivate;
	return FOpenMobileDeviceApplicationInfo::Build(
		CallStringMethod("AndroidThunkJava_OpenMobileDeviceGetDisplayName"),
		CallStringMethod("AndroidThunkJava_OpenMobileDeviceGetPackageIdentifier"),
		CallStringMethod("AndroidThunkJava_OpenMobileDeviceGetVersionName"),
		CallStringMethod("AndroidThunkJava_OpenMobileDeviceGetBuildNumber"),
		FApp::GetBuildConfiguration()
	);
}

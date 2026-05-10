#include "OpenMobileDeviceAndroidIdentity.h"

#include "Android/AndroidApplication.h"

namespace OpenMobileDeviceAndroidIdentityPrivate
{
	bool ClearJavaException(JNIEnv* Env)
	{
		if (!Env->ExceptionCheck())
		{
			return false;
		}
		Env->ExceptionClear();
		return true;
	}

	FString GetBuildStringField(const char* FieldName)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env)
		{
			return {};
		}

		FScopedJavaObject<jclass> BuildClass(Env->FindClass("android/os/Build"));
		const bool bClassError = ClearJavaException(Env);
		if (!BuildClass || bClassError)
		{
			return {};
		}

		const jfieldID Field = Env->GetStaticFieldID(
			*BuildClass,
			FieldName,
			"Ljava/lang/String;"
		);
		const bool bFieldError = ClearJavaException(Env);
		if (!Field || bFieldError)
		{
			return {};
		}

		jstring Value = static_cast<jstring>(
			Env->GetStaticObjectField(*BuildClass, Field)
		);
		if (ClearJavaException(Env))
		{
			return {};
		}
		return FJavaHelper::FStringFromLocalRef(Env, Value);
	}
}

FString GetOpenMobileDeviceAndroidBrand()
{
	return OpenMobileDeviceAndroidIdentityPrivate::GetBuildStringField("BRAND");
}

FString GetOpenMobileDeviceAndroidHardwareModel()
{
	return OpenMobileDeviceAndroidIdentityPrivate::GetBuildStringField("DEVICE");
}

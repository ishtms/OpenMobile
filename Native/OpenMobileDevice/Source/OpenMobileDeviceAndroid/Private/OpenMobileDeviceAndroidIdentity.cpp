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
}

FString GetOpenMobileDeviceAndroidBrand()
{
	using namespace OpenMobileDeviceAndroidIdentityPrivate;
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

	const jfieldID BrandField = Env->GetStaticFieldID(
		*BuildClass,
		"BRAND",
		"Ljava/lang/String;"
	);
	const bool bFieldError = ClearJavaException(Env);
	if (!BrandField || bFieldError)
	{
		return {};
	}

	jstring Brand = static_cast<jstring>(
		Env->GetStaticObjectField(*BuildClass, BrandField)
	);
	if (ClearJavaException(Env))
	{
		return {};
	}
	return FJavaHelper::FStringFromLocalRef(Env, Brand);
}

#include "OpenMobileDeviceAndroidLocale.h"

#include "Android/AndroidApplication.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "OpenMobileDeviceLocaleInfo.h"

FOpenMobileLocaleSnapshot GetOpenMobileDeviceAndroidLocaleSnapshot()
{
	TArray<FString> PreferredLanguages;
	bool bPreferredLanguagesAvailable = false;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (Env && Activity)
	{
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID GetPreferredLanguages = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"AndroidThunkJava_OpenMobileDeviceGetPreferredLanguages",
				"()[Ljava/lang/String;"
			)
			: nullptr;
		const bool bMethodError = Env->ExceptionCheck();
		if (bMethodError)
		{
			Env->ExceptionClear();
		}
		if (GetPreferredLanguages && !bMethodError)
		{
			FScopedJavaObject<jobjectArray> Languages(
				static_cast<jobjectArray>(
					Env->CallObjectMethod(Activity, GetPreferredLanguages)
				)
			);
			const bool bArrayError = Env->ExceptionCheck();
			if (bArrayError)
			{
				Env->ExceptionClear();
			}
			if (Languages && !bArrayError)
			{
				const jsize Count = Env->GetArrayLength(*Languages);
				if (!Env->ExceptionCheck())
				{
					PreferredLanguages.Reserve(Count);
					for (jsize Index = 0; Index < Count; ++Index)
					{
						jstring Language = static_cast<jstring>(
							Env->GetObjectArrayElement(*Languages, Index)
						);
						if (Env->ExceptionCheck())
						{
							Env->ExceptionClear();
							PreferredLanguages.Reset();
							break;
						}
						PreferredLanguages.Add(
							FJavaHelper::FStringFromLocalRef(Env, Language)
						);
					}
					bPreferredLanguagesAvailable =
						PreferredLanguages.Num() == Count;
				}
				else
				{
					Env->ExceptionClear();
				}
			}
		}
	}
	return FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
		PreferredLanguages,
		bPreferredLanguagesAvailable,
		FInternationalization::Get().GetCurrentCulture()->GetName()
	);
}

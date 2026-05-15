#include "OpenMobileDeviceAndroidLocale.h"

#include "Android/AndroidApplication.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "OpenMobileDeviceLocaleInfo.h"
#include "OpenMobileDeviceTimeZoneInfo.h"

namespace OpenMobileDeviceAndroidLocalePrivate
{
	bool CallStringArrayMethod(
		const char* MethodName,
		TArray<FString>& OutValues,
		bool bHasLongArgument = false,
		jlong LongArgument = 0
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
				bHasLongArgument
					? "(J)[Ljava/lang/String;"
					: "()[Ljava/lang/String;"
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
			bHasLongArgument
				? Env->CallObjectMethod(Activity, Method, LongArgument)
				: Env->CallObjectMethod(Activity, Method)
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
}

FOpenMobileLocaleSnapshot GetOpenMobileDeviceAndroidLocaleSnapshot(
	const FDateTime& UtcInstant
)
{
	using namespace OpenMobileDeviceAndroidLocalePrivate;
	TArray<FString> PreferredLanguages;
	const bool bPreferredLanguagesAvailable = CallStringArrayMethod(
		"AndroidThunkJava_OpenMobileDeviceGetPreferredLanguages",
		PreferredLanguages
	);
	FOpenMobileLocaleSnapshot Snapshot =
		FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
			PreferredLanguages,
			bPreferredLanguagesAvailable,
			FInternationalization::Get().GetCurrentCulture()->GetName()
		);
	TArray<FString> LocaleDetails;
	if (CallStringArrayMethod(
		"AndroidThunkJava_OpenMobileDeviceGetLocaleDetails",
		LocaleDetails
	) && LocaleDetails.Num() == 5)
	{
		FOpenMobileDeviceLocaleInfo::ApplyLocale(
			Snapshot,
			LocaleDetails[0],
			LocaleDetails[1],
			LocaleDetails[2],
			LocaleDetails[3],
			LocaleDetails[4]
		);
	}
	TArray<FString> TimeZoneDetails;
	if (CallStringArrayMethod(
		"AndroidThunkJava_OpenMobileDeviceGetTimeZoneAt",
		TimeZoneDetails,
		true,
		static_cast<jlong>(UtcInstant.ToUnixTimestamp()) * 1000
	) && TimeZoneDetails.Num() == 3)
	{
		const bool bOffsetAvailable = TimeZoneDetails[1].IsNumeric();
		const bool bDaylightSavingAvailable =
			TimeZoneDetails[2].Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| TimeZoneDetails[2].Equals(
				TEXT("false"),
				ESearchCase::IgnoreCase
			);
		FOpenMobileDeviceTimeZoneInfo::Apply(
			Snapshot,
			TimeZoneDetails[0],
			bOffsetAvailable ? FCString::Atoi64(*TimeZoneDetails[1]) : 0,
			bOffsetAvailable,
			TimeZoneDetails[2].Equals(
				TEXT("true"),
				ESearchCase::IgnoreCase
			),
			bDaylightSavingAvailable
		);
	}
	return Snapshot;
}

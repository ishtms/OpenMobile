#include "OpenMobileDeviceAndroidIdentity.h"

#include "Android/AndroidApplication.h"
#include "OpenMobileDeviceFormFactor.h"
#include "OpenMobileDeviceEmulatorDetection.h"

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

	bool ReadWindowTraits(
		JNIEnv* Env,
		jobject Activity,
		FOpenMobileDeviceFormFactorTraits& OutTraits
	)
	{
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID GetResources = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"getResources",
				"()Landroid/content/res/Resources;"
			)
			: nullptr;
		const bool bGetResourcesError = ClearJavaException(Env);
		if (!GetResources || bGetResourcesError)
		{
			return false;
		}

		FScopedJavaObject<jobject> Resources(
			Env->CallObjectMethod(Activity, GetResources)
		);
		const bool bResourcesError = ClearJavaException(Env);
		if (!Resources || bResourcesError)
		{
			return false;
		}
		FScopedJavaObject<jclass> ResourcesClass(
			Env->GetObjectClass(*Resources)
		);
		const bool bResourcesClassError = ClearJavaException(Env);
		if (!ResourcesClass || bResourcesClassError)
		{
			return false;
		}
		const jmethodID GetConfiguration = ResourcesClass
			? Env->GetMethodID(
				*ResourcesClass,
				"getConfiguration",
				"()Landroid/content/res/Configuration;"
			)
			: nullptr;
		const bool bGetConfigurationError = ClearJavaException(Env);
		if (!GetConfiguration || bGetConfigurationError)
		{
			return false;
		}

		FScopedJavaObject<jobject> Configuration(
			Env->CallObjectMethod(*Resources, GetConfiguration)
		);
		const bool bConfigurationError = ClearJavaException(Env);
		if (!Configuration || bConfigurationError)
		{
			return false;
		}
		FScopedJavaObject<jclass> ConfigurationClass(
			Env->GetObjectClass(*Configuration)
		);
		const bool bConfigurationClassError = ClearJavaException(Env);
		if (!ConfigurationClass || bConfigurationClassError)
		{
			return false;
		}
		const jfieldID SmallestWidth = Env->GetFieldID(
			*ConfigurationClass,
			"smallestScreenWidthDp",
			"I"
		);
		const bool bSmallestWidthError = ClearJavaException(Env);
		if (!SmallestWidth || bSmallestWidthError)
		{
			return false;
		}
		const jfieldID ScreenLayout = Env->GetFieldID(
			*ConfigurationClass,
			"screenLayout",
			"I"
		);
		const bool bScreenLayoutError = ClearJavaException(Env);
		if (!ScreenLayout || bScreenLayoutError)
		{
			return false;
		}

		OutTraits.SmallestWindowWidthDp = Env->GetIntField(
			*Configuration,
			SmallestWidth
		);
		if (ClearJavaException(Env))
		{
			return false;
		}
		const int32 SizeClass = Env->GetIntField(
			*Configuration,
			ScreenLayout
		) & 0x0f;
		if (ClearJavaException(Env))
		{
			return false;
		}
		if (SizeClass == 1 || SizeClass == 2)
		{
			OutTraits.WindowSizeClass =
				EOpenMobileDeviceWindowSizeClass::Compact;
		}
		else if (SizeClass == 3 || SizeClass == 4)
		{
			OutTraits.WindowSizeClass =
				EOpenMobileDeviceWindowSizeClass::Large;
		}
		return true;
	}

	bool HasHingeSensor(JNIEnv* Env, jobject Activity)
	{
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID GetPackageManager = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"getPackageManager",
				"()Landroid/content/pm/PackageManager;"
			)
			: nullptr;
		const bool bGetPackageManagerError = ClearJavaException(Env);
		if (!GetPackageManager || bGetPackageManagerError)
		{
			return false;
		}

		FScopedJavaObject<jobject> PackageManager(
			Env->CallObjectMethod(Activity, GetPackageManager)
		);
		const bool bPackageManagerError = ClearJavaException(Env);
		if (!PackageManager || bPackageManagerError)
		{
			return false;
		}
		FScopedJavaObject<jclass> PackageManagerClass(
			Env->GetObjectClass(*PackageManager)
		);
		const bool bPackageManagerClassError = ClearJavaException(Env);
		if (!PackageManagerClass || bPackageManagerClassError)
		{
			return false;
		}
		const jmethodID HasSystemFeature = PackageManagerClass
			? Env->GetMethodID(
				*PackageManagerClass,
				"hasSystemFeature",
				"(Ljava/lang/String;)Z"
			)
			: nullptr;
		const bool bHasSystemFeatureError = ClearJavaException(Env);
		if (!HasSystemFeature || bHasSystemFeatureError)
		{
			return false;
		}

		FScopedJavaObject<jstring> Feature = FJavaHelper::ToJavaString(
			Env,
			TEXT("android.hardware.sensor.hinge_angle")
		);
		const bool bFeatureError = ClearJavaException(Env);
		if (!Feature || bFeatureError)
		{
			return false;
		}
		const bool bHasFeature = Env->CallBooleanMethod(
			*PackageManager,
			HasSystemFeature,
			*Feature
		);
		return ClearJavaException(Env) ? false : bHasFeature;
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

EOpenMobileDeviceFormFactor GetOpenMobileDeviceAndroidFormFactor()
{
	using namespace OpenMobileDeviceAndroidIdentityPrivate;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		return EOpenMobileDeviceFormFactor::Unknown;
	}

	FOpenMobileDeviceFormFactorTraits Traits;
	ReadWindowTraits(Env, Activity, Traits);
	Traits.bHasFoldableHardware = HasHingeSensor(Env, Activity);
	return FOpenMobileDeviceFormFactor::Classify(Traits);
}

bool GetOpenMobileDeviceAndroidSupportedAbis(
	TArray<FString>& OutSupportedAbis
)
{
	using namespace OpenMobileDeviceAndroidIdentityPrivate;
	OutSupportedAbis.Reset();
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		return false;
	}

	FScopedJavaObject<jclass> BuildClass(Env->FindClass("android/os/Build"));
	const bool bClassError = ClearJavaException(Env);
	if (!BuildClass || bClassError)
	{
		return false;
	}
	const jfieldID SupportedAbisField = Env->GetStaticFieldID(
		*BuildClass,
		"SUPPORTED_ABIS",
		"[Ljava/lang/String;"
	);
	const bool bFieldError = ClearJavaException(Env);
	if (!SupportedAbisField || bFieldError)
	{
		return false;
	}

	FScopedJavaObject<jobjectArray> SupportedAbis(
		static_cast<jobjectArray>(
			Env->GetStaticObjectField(*BuildClass, SupportedAbisField)
		)
	);
	const bool bArrayError = ClearJavaException(Env);
	if (!SupportedAbis || bArrayError)
	{
		return false;
	}
	const jsize Count = Env->GetArrayLength(*SupportedAbis);
	if (ClearJavaException(Env))
	{
		return false;
	}
	OutSupportedAbis.Reserve(Count);
	for (jsize Index = 0; Index < Count; ++Index)
	{
		jstring Abi = static_cast<jstring>(
			Env->GetObjectArrayElement(*SupportedAbis, Index)
		);
		if (ClearJavaException(Env))
		{
			OutSupportedAbis.Reset();
			return false;
		}
		OutSupportedAbis.Add(FJavaHelper::FStringFromLocalRef(Env, Abi));
	}
	return true;
}

void ApplyOpenMobileDeviceAndroidEmulatorDetection(
	FOpenMobileDeviceInformationSnapshot& Snapshot
)
{
	using namespace OpenMobileDeviceAndroidIdentityPrivate;
	FOpenMobileDeviceAndroidEmulatorEvidence Evidence;
	Evidence.Manufacturer = Snapshot.Manufacturer.bIsAvailable
		? Snapshot.Manufacturer.Value
		: FString();
	Evidence.Brand = Snapshot.Brand.bIsAvailable
		? Snapshot.Brand.Value
		: FString();
	Evidence.Model = Snapshot.Model.bIsAvailable
		? Snapshot.Model.Value
		: FString();
	Evidence.Device = Snapshot.HardwareModelIdentifier.bIsAvailable
		? Snapshot.HardwareModelIdentifier.Value
		: FString();
	Evidence.Hardware = GetBuildStringField("HARDWARE");
	Evidence.Product = GetBuildStringField("PRODUCT");
	Evidence.Fingerprint = GetBuildStringField("FINGERPRINT");
	FOpenMobileDeviceEmulatorDetection::ApplyAndroid(Snapshot, Evidence);
}

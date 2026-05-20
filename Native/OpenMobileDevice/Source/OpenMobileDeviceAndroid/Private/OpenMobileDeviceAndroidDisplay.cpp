#include "OpenMobileDeviceAndroidDisplay.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidPlatformMisc.h"
#include "OpenMobileDeviceWindowMetrics.h"

namespace OpenMobileDeviceAndroidDisplayPrivate
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

	bool ReadResourceMetrics(
		JNIEnv* Env,
		jobject Activity,
		FOpenMobileDeviceWindowMetricsEvidence& Evidence
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
		if (!GetResources || ClearJavaException(Env))
		{
			return false;
		}
		FScopedJavaObject<jobject> Resources(
			Env->CallObjectMethod(Activity, GetResources)
		);
		if (!Resources || ClearJavaException(Env))
		{
			return false;
		}
		FScopedJavaObject<jclass> ResourcesClass(
			Env->GetObjectClass(*Resources)
		);
		const jmethodID GetConfiguration = ResourcesClass
			? Env->GetMethodID(
				*ResourcesClass,
				"getConfiguration",
				"()Landroid/content/res/Configuration;"
			)
			: nullptr;
		const jmethodID GetDisplayMetrics = ResourcesClass
			? Env->GetMethodID(
				*ResourcesClass,
				"getDisplayMetrics",
				"()Landroid/util/DisplayMetrics;"
			)
			: nullptr;
		if (!GetConfiguration || !GetDisplayMetrics || ClearJavaException(Env))
		{
			return false;
		}

		FScopedJavaObject<jobject> Configuration(
			Env->CallObjectMethod(*Resources, GetConfiguration)
		);
		FScopedJavaObject<jobject> DisplayMetrics(
			Env->CallObjectMethod(*Resources, GetDisplayMetrics)
		);
		if (!Configuration || !DisplayMetrics || ClearJavaException(Env))
		{
			return false;
		}
		FScopedJavaObject<jclass> ConfigurationClass(
			Env->GetObjectClass(*Configuration)
		);
		FScopedJavaObject<jclass> DisplayMetricsClass(
			Env->GetObjectClass(*DisplayMetrics)
		);
		const jfieldID ScreenWidthDp = ConfigurationClass
			? Env->GetFieldID(*ConfigurationClass, "screenWidthDp", "I")
			: nullptr;
		const jfieldID ScreenHeightDp = ConfigurationClass
			? Env->GetFieldID(*ConfigurationClass, "screenHeightDp", "I")
			: nullptr;
		const jfieldID Density = DisplayMetricsClass
			? Env->GetFieldID(*DisplayMetricsClass, "density", "F")
			: nullptr;
		const jfieldID DensityDpi = DisplayMetricsClass
			? Env->GetFieldID(*DisplayMetricsClass, "densityDpi", "I")
			: nullptr;
		if (!ScreenWidthDp || !ScreenHeightDp || !Density || !DensityDpi
			|| ClearJavaException(Env))
		{
			return false;
		}

		Evidence.LogicalWindowSize = FVector2D(
			Env->GetIntField(*Configuration, ScreenWidthDp),
			Env->GetIntField(*Configuration, ScreenHeightDp)
		);
		Evidence.ScaleFactor = Env->GetFloatField(*DisplayMetrics, Density);
		Evidence.DensityDpi = static_cast<float>(
			Env->GetIntField(*DisplayMetrics, DensityDpi)
		);
		return !ClearJavaException(Env);
	}

	void ReadDisplayAndWindowMode(
		JNIEnv* Env,
		jobject Activity,
		FOpenMobileDeviceWindowMetricsEvidence& Evidence
	)
	{
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID GetWindowManager = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"getWindowManager",
				"()Landroid/view/WindowManager;"
			)
			: nullptr;
		if (!GetWindowManager || ClearJavaException(Env))
		{
			return;
		}
		FScopedJavaObject<jobject> WindowManager(
			Env->CallObjectMethod(Activity, GetWindowManager)
		);
		if (!WindowManager || ClearJavaException(Env))
		{
			return;
		}
		FScopedJavaObject<jclass> WindowManagerClass(
			Env->GetObjectClass(*WindowManager)
		);
		const jmethodID GetDefaultDisplay = WindowManagerClass
			? Env->GetMethodID(
				*WindowManagerClass,
				"getDefaultDisplay",
				"()Landroid/view/Display;"
			)
			: nullptr;
		if (GetDefaultDisplay && !ClearJavaException(Env))
		{
			FScopedJavaObject<jobject> Display(
				Env->CallObjectMethod(*WindowManager, GetDefaultDisplay)
			);
			FScopedJavaObject<jclass> DisplayClass(
				Display ? Env->GetObjectClass(*Display) : nullptr
			);
			const jmethodID GetDisplayId = DisplayClass
				? Env->GetMethodID(*DisplayClass, "getDisplayId", "()I")
				: nullptr;
			if (Display && GetDisplayId && !ClearJavaException(Env))
			{
				const int32 DisplayId = Env->CallIntMethod(
					*Display,
					GetDisplayId
				);
				if (!ClearJavaException(Env))
				{
					Evidence.ScreenIdentifier = FString::Printf(
						TEXT("Android:%d"),
						DisplayId
					);
				}
			}
		}

		if (FAndroidMisc::GetAndroidBuildVersion() >= 24)
		{
			const jmethodID IsInMultiWindowMode = ActivityClass
				? Env->GetMethodID(
					*ActivityClass,
					"isInMultiWindowMode",
					"()Z"
				)
				: nullptr;
			if (IsInMultiWindowMode && !ClearJavaException(Env))
			{
				Evidence.bIsWindowed = Env->CallBooleanMethod(
					Activity,
					IsInMultiWindowMode
				) == JNI_TRUE;
				ClearJavaException(Env);
			}
		}
		else
		{
			Evidence.bIsWindowed = false;
		}
	}
}

FOpenMobileWindowDisplaySnapshot
GetOpenMobileDeviceAndroidWindowDisplaySnapshot()
{
	FOpenMobileDeviceWindowMetricsEvidence Evidence;
	if (FAndroidApplication* Application = FAndroidApplication::Get())
	{
		int32 Width = 0;
		int32 Height = 0;
		if (Application->GetNativeWindowResolution(Width, Height))
		{
			Evidence.DrawablePixelSize = FIntPoint(Width, Height);
		}
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (Env && Activity)
	{
		OpenMobileDeviceAndroidDisplayPrivate::ReadResourceMetrics(
			Env,
			Activity,
			Evidence
		);
		OpenMobileDeviceAndroidDisplayPrivate::ReadDisplayAndWindowMode(
			Env,
			Activity,
			Evidence
		);
	}
	return FOpenMobileDeviceWindowMetrics::Build(Evidence);
}

#include "OpenMobileDeviceAndroidDisplay.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidPlatformMisc.h"
#include "OpenMobileDeviceRefreshRateInfo.h"
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

	void ReadRefreshRateInfo(
		JNIEnv* Env,
		jobject Display,
		jclass DisplayClass,
		FOpenMobileDeviceRefreshRateEvidence& Evidence
	)
	{
		const jmethodID GetRefreshRate = Env->GetMethodID(
			DisplayClass,
			"getRefreshRate",
			"()F"
		);
		if (GetRefreshRate && !ClearJavaException(Env))
		{
			Evidence.CurrentRefreshRateHz = Env->CallFloatMethod(
				Display,
				GetRefreshRate
			);
			ClearJavaException(Env);
		}

		const jmethodID GetSupportedModes = Env->GetMethodID(
			DisplayClass,
			"getSupportedModes",
			"()[Landroid/view/Display$Mode;"
		);
		if (!GetSupportedModes || ClearJavaException(Env))
		{
			return;
		}
		FScopedJavaObject<jobjectArray> Modes(static_cast<jobjectArray>(
			Env->CallObjectMethod(Display, GetSupportedModes)
		));
		if (!Modes || ClearJavaException(Env))
		{
			return;
		}
		Evidence.bSupportedModesAvailable = true;
		const bool bAlternativeRatesAvailable =
			FAndroidMisc::GetAndroidBuildVersion() >= 31;
		const jsize ModeCount = Env->GetArrayLength(*Modes);
		for (jsize ModeIndex = 0; ModeIndex < ModeCount; ++ModeIndex)
		{
			FScopedJavaObject<jobject> Mode(
				Env->GetObjectArrayElement(*Modes, ModeIndex)
			);
			if (!Mode || ClearJavaException(Env))
			{
				continue;
			}
			FScopedJavaObject<jclass> ModeClass(Env->GetObjectClass(*Mode));
			const jmethodID GetPhysicalWidth = ModeClass
				? Env->GetMethodID(*ModeClass, "getPhysicalWidth", "()I")
				: nullptr;
			const jmethodID GetPhysicalHeight = ModeClass
				? Env->GetMethodID(*ModeClass, "getPhysicalHeight", "()I")
				: nullptr;
			const jmethodID GetModeRefreshRate = ModeClass
				? Env->GetMethodID(*ModeClass, "getRefreshRate", "()F")
				: nullptr;
			if (!GetPhysicalWidth || !GetPhysicalHeight || !GetModeRefreshRate
				|| ClearJavaException(Env))
			{
				continue;
			}
			FOpenMobileDeviceRefreshModeEvidence NativeMode;
			NativeMode.PixelSize = FIntPoint(
				Env->CallIntMethod(*Mode, GetPhysicalWidth),
				Env->CallIntMethod(*Mode, GetPhysicalHeight)
			);
			NativeMode.RefreshRatesHz.Add(
				Env->CallFloatMethod(*Mode, GetModeRefreshRate)
			);
			if (ClearJavaException(Env))
			{
				continue;
			}

			if (bAlternativeRatesAvailable)
			{
				const jmethodID GetAlternativeRefreshRates =
					Env->GetMethodID(
						*ModeClass,
						"getAlternativeRefreshRates",
						"()[F"
					);
				if (GetAlternativeRefreshRates && !ClearJavaException(Env))
				{
					FScopedJavaObject<jfloatArray> AlternativeRates(
						static_cast<jfloatArray>(Env->CallObjectMethod(
							*Mode,
							GetAlternativeRefreshRates
						))
					);
					if (AlternativeRates && !ClearJavaException(Env))
					{
						const jsize RateCount = Env->GetArrayLength(
							*AlternativeRates
						);
						TArray<jfloat> Rates;
						Rates.SetNumUninitialized(RateCount);
						Env->GetFloatArrayRegion(
							*AlternativeRates,
							0,
							RateCount,
							Rates.GetData()
						);
						if (!ClearJavaException(Env))
						{
							for (const jfloat Rate : Rates)
							{
								NativeMode.RefreshRatesHz.Add(Rate);
							}
						}
					}
				}
			}
			Evidence.SupportedModes.Add(MoveTemp(NativeMode));
		}
	}

	void ReadDisplayAndWindowMode(
		JNIEnv* Env,
		jobject Activity,
		FOpenMobileDeviceWindowMetricsEvidence& Evidence,
		FOpenMobileDeviceRefreshRateEvidence& RefreshRateEvidence
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
			if (Display && DisplayClass && !ClearJavaException(Env))
			{
				ReadRefreshRateInfo(
					Env,
					*Display,
					*DisplayClass,
					RefreshRateEvidence
				);
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
	FOpenMobileDeviceRefreshRateEvidence RefreshRateEvidence;
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
			Evidence,
			RefreshRateEvidence
		);
	}
	FOpenMobileWindowDisplaySnapshot Snapshot =
		FOpenMobileDeviceWindowMetrics::Build(Evidence);
	FOpenMobileDeviceRefreshRateInfo::Apply(Snapshot, RefreshRateEvidence);
	return Snapshot;
}

FOpenMobilePreferredRefreshRateResult
ApplyOpenMobileDeviceAndroidPreferredRefreshRate(
	const FOpenMobilePreferredRefreshRateRequest& Request
)
{
	FOpenMobilePreferredRefreshRateResult Result;
	Result.Request = Request;
	if (Request.bUsePreferredMinimumHz || Request.bUsePreferredMaximumHz
		|| !Request.bUsePreferredTargetHz)
	{
		Result.State = EOpenMobilePreferredRefreshRateApplyState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("Android supports an explicit preferred target, but not a preferred refresh-rate range through the public Window API."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		Result.State = EOpenMobilePreferredRefreshRateApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android activity is unavailable for the refresh-rate request."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}

	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceApplyPreferredRefreshRate",
			"(F)Z"
		)
		: nullptr;
	const bool bMethodError =
		OpenMobileDeviceAndroidDisplayPrivate::ClearJavaException(Env);
	if (!Method || bMethodError)
	{
		Result.State = EOpenMobilePreferredRefreshRateApplyState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android refresh-rate request method is unavailable."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
	const bool bAccepted = Env->CallBooleanMethod(
		Activity,
		Method,
		Request.PreferredTargetHz
	) == JNI_TRUE;
	const bool bCallError =
		OpenMobileDeviceAndroidDisplayPrivate::ClearJavaException(Env);
	Result.State = bAccepted && !bCallError
		? EOpenMobilePreferredRefreshRateApplyState::Accepted
		: EOpenMobilePreferredRefreshRateApplyState::Rejected;
	if (!Result.IsAccepted())
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android rejected the preferred refresh-rate request."),
			FString(),
			TEXT("Android")
		);
	}
	return Result;
}

void ClearOpenMobileDeviceAndroidPreferredRefreshRate()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		return;
	}
	FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
	const jmethodID Method = ActivityClass
		? Env->GetMethodID(
			*ActivityClass,
			"AndroidThunkJava_OpenMobileDeviceClearPreferredRefreshRate",
			"()V"
		)
		: nullptr;
	if (!Method || OpenMobileDeviceAndroidDisplayPrivate::ClearJavaException(Env))
	{
		return;
	}
	Env->CallVoidMethod(Activity, Method);
	OpenMobileDeviceAndroidDisplayPrivate::ClearJavaException(Env);
}

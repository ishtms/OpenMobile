#include "OpenMobileDeviceAndroidDisplay.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidPlatformMisc.h"
#include "OpenMobileDeviceDisplayCutoutInfo.h"
#include "OpenMobileDeviceRefreshRateInfo.h"
#include "OpenMobileDeviceWindowInsets.h"
#include "OpenMobileDeviceWindowMetrics.h"
#include "OpenMobileDeviceWindowMode.h"
#include "OpenMobileDeviceWindowOrientation.h"

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

		bool bIsInMultiWindowMode = false;
		bool bMultiWindowStateAvailable = false;
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
				bIsInMultiWindowMode = Env->CallBooleanMethod(
					Activity,
					IsInMultiWindowMode
				) == JNI_TRUE;
				bMultiWindowStateAvailable = !ClearJavaException(Env);
			}
		}
		else
		{
			bMultiWindowStateAvailable = true;
		}

		bool bIsInPictureInPictureMode = false;
		bool bPictureInPictureStateAvailable =
			FAndroidMisc::GetAndroidBuildVersion() < 26;
		if (FAndroidMisc::GetAndroidBuildVersion() >= 26)
		{
			const jmethodID IsInPictureInPictureMode = ActivityClass
				? Env->GetMethodID(
					*ActivityClass,
					"isInPictureInPictureMode",
					"()Z"
				)
				: nullptr;
			if (IsInPictureInPictureMode && !ClearJavaException(Env))
			{
				bIsInPictureInPictureMode = Env->CallBooleanMethod(
					Activity,
					IsInPictureInPictureMode
				) == JNI_TRUE;
				bPictureInPictureStateAvailable = !ClearJavaException(Env);
			}
		}
		if (bMultiWindowStateAvailable && bPictureInPictureStateAvailable)
		{
			Evidence.bIsWindowed = bIsInMultiWindowMode
				|| bIsInPictureInPictureMode;
			Evidence.WindowMode = FOpenMobileDeviceWindowMode::FromAndroid(
				bIsInMultiWindowMode,
				bIsInPictureInPictureMode
			);
		}
	}

	void ApplyOpenMobileDeviceAndroidWindowInsets(
		JNIEnv* Env,
		jobject Activity,
		FOpenMobileWindowDisplaySnapshot& Snapshot
	)
	{
		FOpenMobileDeviceWindowInsetsEvidence Evidence;
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID GetWindowInsets = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"AndroidThunkJava_OpenMobileDeviceGetWindowInsets",
				"()[F"
			)
			: nullptr;
		if (!GetWindowInsets || ClearJavaException(Env))
		{
			FOpenMobileDeviceWindowInsets::Apply(Snapshot, Evidence);
			return;
		}
		FScopedJavaObject<jfloatArray> NativeValues(
			static_cast<jfloatArray>(
				Env->CallObjectMethod(Activity, GetWindowInsets)
			)
		);
		if (!NativeValues || ClearJavaException(Env)
			|| Env->GetArrayLength(*NativeValues) != 16)
		{
			FOpenMobileDeviceWindowInsets::Apply(Snapshot, Evidence);
			return;
		}
		TArray<jfloat> Values;
		Values.SetNumUninitialized(16);
		Env->GetFloatArrayRegion(*NativeValues, 0, 16, Values.GetData());
		if (ClearJavaException(Env) || !FMath::IsFinite(Values[0])
			|| Values[0] <= 0.0f)
		{
			FOpenMobileDeviceWindowInsets::Apply(Snapshot, Evidence);
			return;
		}
		const float Density = Values[0];
		auto ReadInsets = [&Values, Density](int32 AvailabilityIndex)
			-> TOptional<FOpenMobileDeviceInsetValues>
		{
			if (Values[AvailabilityIndex] < 0.5f)
			{
				return {};
			}
			const int32 FirstValueIndex = AvailabilityIndex + 1;
			return FOpenMobileDeviceInsetValues{
				Values[FirstValueIndex] / Density,
				Values[FirstValueIndex + 1] / Density,
				Values[FirstValueIndex + 2] / Density,
				Values[FirstValueIndex + 3] / Density
			};
		};
		Evidence.SafeArea = ReadInsets(1);
		Evidence.SystemBars = ReadInsets(6);
		Evidence.SystemGestures = ReadInsets(11);
		FOpenMobileDeviceWindowInsets::Apply(Snapshot, Evidence);
	}

	void ApplyOpenMobileDeviceAndroidDisplayCutout(
		JNIEnv* Env,
		jobject Activity,
		FOpenMobileWindowDisplaySnapshot& Snapshot
	)
	{
		FOpenMobileDeviceDisplayCutoutEvidence Evidence;
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID GetDisplayCutouts = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"AndroidThunkJava_OpenMobileDeviceGetDisplayCutouts",
				"()[F"
			)
			: nullptr;
		if (!GetDisplayCutouts || ClearJavaException(Env))
		{
			FOpenMobileDeviceDisplayCutoutInfo::Apply(Snapshot, Evidence);
			return;
		}
		FScopedJavaObject<jfloatArray> NativeValues(
			static_cast<jfloatArray>(
				Env->CallObjectMethod(Activity, GetDisplayCutouts)
			)
		);
		if (!NativeValues || ClearJavaException(Env))
		{
			FOpenMobileDeviceDisplayCutoutInfo::Apply(Snapshot, Evidence);
			return;
		}
		const jsize ValueCount = Env->GetArrayLength(*NativeValues);
		if (ValueCount < 10)
		{
			FOpenMobileDeviceDisplayCutoutInfo::Apply(Snapshot, Evidence);
			return;
		}
		TArray<jfloat> Values;
		Values.SetNumUninitialized(ValueCount);
		Env->GetFloatArrayRegion(
			*NativeValues,
			0,
			ValueCount,
			Values.GetData()
		);
		if (ClearJavaException(Env)
			|| !FMath::IsFinite(Values[0])
			|| Values[0] <= 0.0f
			|| !FMath::IsFinite(Values[9]))
		{
			FOpenMobileDeviceDisplayCutoutInfo::Apply(Snapshot, Evidence);
			return;
		}
		const int32 CutoutCount = FMath::RoundToInt(Values[9]);
		if (CutoutCount < 0 || CutoutCount > (ValueCount - 10) / 4
			|| ValueCount != 10 + CutoutCount * 4)
		{
			FOpenMobileDeviceDisplayCutoutInfo::Apply(Snapshot, Evidence);
			return;
		}
		Evidence.NativeUnitsPerLogicalUnit = Values[0];
		Evidence.NativeWindowOrigin = FVector2D(Values[1], Values[2]);
		Evidence.bCutoutsAvailable = Values[3] >= 0.5f;
		if (Values[4] >= 0.5f)
		{
			Evidence.NativeWaterfallInsets = FOpenMobileDeviceInsetValues{
				Values[5],
				Values[6],
				Values[7],
				Values[8]
			};
		}
		for (int32 Index = 0; Index < CutoutCount; ++Index)
		{
			const int32 FirstValue = 10 + Index * 4;
			FOpenMobileDeviceRect Rect;
			Rect.Left = Values[FirstValue];
			Rect.Top = Values[FirstValue + 1];
			Rect.Right = Values[FirstValue + 2];
			Rect.Bottom = Values[FirstValue + 3];
			Evidence.NativeCutouts.Add(Rect);
		}
		FOpenMobileDeviceDisplayCutoutInfo::Apply(Snapshot, Evidence);
	}

	void ApplyOpenMobileDeviceAndroidWindowOrientation(
		JNIEnv* Env,
		jobject Activity,
		FOpenMobileWindowDisplaySnapshot& Snapshot
	)
	{
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID GetWindowOrientation = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"AndroidThunkJava_OpenMobileDeviceGetWindowOrientation",
				"()[I"
			)
			: nullptr;
		if (!GetWindowOrientation || ClearJavaException(Env))
		{
			return;
		}
		FScopedJavaObject<jintArray> NativeValues(
			static_cast<jintArray>(
				Env->CallObjectMethod(Activity, GetWindowOrientation)
			)
		);
		if (!NativeValues || ClearJavaException(Env)
			|| Env->GetArrayLength(*NativeValues) != 2)
		{
			return;
		}
		jint Values[2] = {};
		Env->GetIntArrayRegion(*NativeValues, 0, 2, Values);
		if (ClearJavaException(Env) || (Values[1] != 0 && Values[1] != 1))
		{
			return;
		}
		Snapshot.Orientation =
			FOpenMobileDeviceWindowOrientation::FromAndroidRotation(
				Values[0],
				Values[1] == 1
			);
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
	if (Env && Activity)
	{
		OpenMobileDeviceAndroidDisplayPrivate::
			ApplyOpenMobileDeviceAndroidWindowInsets(
				Env,
				Activity,
				Snapshot
			);
		OpenMobileDeviceAndroidDisplayPrivate::
			ApplyOpenMobileDeviceAndroidDisplayCutout(
				Env,
				Activity,
				Snapshot
			);
		OpenMobileDeviceAndroidDisplayPrivate::
			ApplyOpenMobileDeviceAndroidWindowOrientation(
				Env,
				Activity,
				Snapshot
			);
	}
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

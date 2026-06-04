#include "OpenMobileDeviceAndroidBrightnessControl.h"

#include "Android/AndroidApplication.h"

namespace OpenMobileDeviceAndroidBrightnessControlPrivate
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

	TOptional<float> CallBrightnessArrayMethod(
		const char* MethodName,
		const char* Signature,
		TOptional<float> Argument = {}
	)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Env || !Activity)
		{
			return {};
		}
		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID Method = ActivityClass
			? Env->GetMethodID(*ActivityClass, MethodName, Signature)
			: nullptr;
		if (!Method || ClearJavaException(Env))
		{
			return {};
		}
		jfloatArray Values = Argument.IsSet()
			? static_cast<jfloatArray>(Env->CallObjectMethod(
				Activity,
				Method,
				static_cast<jfloat>(Argument.GetValue())
			))
			: static_cast<jfloatArray>(Env->CallObjectMethod(Activity, Method));
		if (ClearJavaException(Env) || !Values)
		{
			return {};
		}
		TOptional<float> Result;
		if (Env->GetArrayLength(Values) > 0)
		{
			jfloat Value = 0.0f;
			Env->GetFloatArrayRegion(Values, 0, 1, &Value);
			if (!ClearJavaException(Env) && FMath::IsFinite(Value)
				&& Value >= 0.0f && Value <= 1.0f)
			{
				Result = Value;
			}
		}
		Env->DeleteLocalRef(Values);
		return Result;
	}
}

FOpenMobileBrightnessSnapshot GetOpenMobileDeviceAndroidBrightnessSnapshot()
{
	FOpenMobileBrightnessSnapshot Snapshot;
	const TOptional<float> Brightness =
		OpenMobileDeviceAndroidBrightnessControlPrivate::CallBrightnessArrayMethod(
			"AndroidThunkJava_OpenMobileDeviceGetBrightness",
			"()[F"
		);
	if (Brightness.IsSet())
	{
		Snapshot.CurrentBrightness =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(
				Brightness.GetValue()
			);
	}
	return Snapshot;
}

FOpenMobileBrightnessResult ApplyOpenMobileDeviceAndroidBrightness(
	const FOpenMobileBrightnessRequest& Request
)
{
	FOpenMobileBrightnessResult Result;
	Result.Request = Request;
	const TOptional<float> Effective =
		OpenMobileDeviceAndroidBrightnessControlPrivate::CallBrightnessArrayMethod(
			"AndroidThunkJava_OpenMobileDeviceApplyBrightness",
			"(F)[F",
			Request.Brightness
		);
	if (Effective.IsSet())
	{
		Result.State = EOpenMobileBrightnessApplyState::Applied;
		Result.EffectiveBrightness =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(Effective.GetValue());
		return Result;
	}
	Result.State = EOpenMobileBrightnessApplyState::Rejected;
	Result.Error = FOpenMobileError::Make(
		EOpenMobileErrorCode::NativeFailure,
		TEXT("Android could not apply or read back the app-window brightness."),
		FString(),
		TEXT("Android")
	);
	return Result;
}

void ClearOpenMobileDeviceAndroidBrightness()
{
	using namespace OpenMobileDeviceAndroidBrightnessControlPrivate;
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
			"AndroidThunkJava_OpenMobileDeviceClearBrightness",
			"()V"
		)
		: nullptr;
	if (!Method || ClearJavaException(Env))
	{
		return;
	}
	Env->CallVoidMethod(Activity, Method);
	ClearJavaException(Env);
}

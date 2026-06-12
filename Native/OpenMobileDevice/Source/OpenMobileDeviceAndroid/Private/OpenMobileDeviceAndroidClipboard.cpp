#include "OpenMobileDeviceAndroidClipboard.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

namespace OpenMobileDeviceAndroidClipboardPrivate
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

	bool ReadStringArray(
		JNIEnv* Env,
		jobjectArray Values,
		TArray<FString>& OutValues
	)
	{
		OutValues.Reset();
		if (!Values)
		{
			return false;
		}
		const jsize Count = Env->GetArrayLength(Values);
		if (ClearJavaException(Env))
		{
			return false;
		}
		OutValues.Reserve(Count);
		for (jsize Index = 0; Index < Count; ++Index)
		{
			jstring Value = static_cast<jstring>(
				Env->GetObjectArrayElement(Values, Index)
			);
			if (ClearJavaException(Env))
			{
				OutValues.Reset();
				return false;
			}
			OutValues.Add(FJavaHelper::FStringFromLocalRef(Env, Value));
		}
		return true;
	}

	bool CallNoArgMethod(
		const char* MethodName,
		TArray<FString>& OutValues
	)
	{
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
				"()[Ljava/lang/String;"
			)
			: nullptr;
		if (!Method || ClearJavaException(Env))
		{
			return false;
		}
		FScopedJavaObject<jobjectArray> Values(static_cast<jobjectArray>(
			Env->CallObjectMethod(Activity, Method)
		));
		return !ClearJavaException(Env)
			&& ReadStringArray(Env, *Values, OutValues);
	}

	bool CallReadMethod(
		EOpenMobileClipboardContentType ContentType,
		TArray<FString>& OutValues
	)
	{
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
				"AndroidThunkJava_OpenMobileDeviceReadClipboard",
				"(I)[Ljava/lang/String;"
			)
			: nullptr;
		if (!Method || ClearJavaException(Env))
		{
			return false;
		}
		FScopedJavaObject<jobjectArray> Values(static_cast<jobjectArray>(
			Env->CallObjectMethod(
				Activity,
				Method,
				static_cast<jint>(ContentType)
			)
		));
		return !ClearJavaException(Env)
			&& ReadStringArray(Env, *Values, OutValues);
	}

	bool CallWriteMethod(
		const FOpenMobileClipboardWriteRequest& Request,
		TArray<FString>& OutValues
	)
	{
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
				"AndroidThunkJava_OpenMobileDeviceWriteClipboard",
				"(ILjava/lang/String;)[Ljava/lang/String;"
			)
			: nullptr;
		if (!Method || ClearJavaException(Env))
		{
			return false;
		}
		FScopedJavaObject<jstring> Value = FJavaHelper::ToJavaString(
			Env,
			Request.Value
		);
		FScopedJavaObject<jobjectArray> Values(static_cast<jobjectArray>(
			Env->CallObjectMethod(
				Activity,
				Method,
				static_cast<jint>(Request.ContentType),
				*Value
			)
		));
		return !ClearJavaException(Env)
			&& ReadStringArray(Env, *Values, OutValues);
	}

	void ApplyState(
		const FString& State,
		const FString& NativeCode,
		FOpenMobileClipboardOperationResult& Result
	)
	{
		if (State == TEXT("succeeded"))
		{
			Result.State = EOpenMobileClipboardOperationState::Succeeded;
			return;
		}
		if (State == TEXT("empty"))
		{
			Result.State = EOpenMobileClipboardOperationState::Empty;
			return;
		}
		EOpenMobileErrorCode ErrorCode = EOpenMobileErrorCode::NativeFailure;
		FString Message = TEXT("Android clipboard operation failed.");
		if (State == TEXT("type_unavailable"))
		{
			Result.State = EOpenMobileClipboardOperationState::TypeUnavailable;
			Message = TEXT("The requested clipboard type is not available.");
		}
		else if (State == TEXT("unavailable"))
		{
			Result.State = EOpenMobileClipboardOperationState::Unavailable;
			ErrorCode = EOpenMobileErrorCode::Unavailable;
			Message = TEXT("Android clipboard access requires input focus.");
		}
		else if (State == TEXT("unsupported"))
		{
			Result.State = EOpenMobileClipboardOperationState::Unsupported;
			ErrorCode = EOpenMobileErrorCode::NotSupported;
			Message = TEXT("Android does not support this clipboard operation.");
		}
		else if (State == TEXT("denied"))
		{
			Result.State = EOpenMobileClipboardOperationState::Denied;
			Message = TEXT("Android denied clipboard access.");
		}
		else
		{
			Result.State = EOpenMobileClipboardOperationState::Failed;
		}
		Result.Error = FOpenMobileError::Make(
			ErrorCode,
			MoveTemp(Message),
			NativeCode,
			TEXT("Android")
		);
	}

	FOpenMobileClipboardOperationResult MakeBridgeFailure()
	{
		FOpenMobileClipboardOperationResult Result;
		Result.State = EOpenMobileClipboardOperationState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android clipboard bridge was unavailable."),
			FString(),
			TEXT("Android")
		);
		return Result;
	}
}

FOpenMobileClipboardOperationResult
CheckOpenMobileDeviceAndroidClipboardContentTypes()
{
	using namespace OpenMobileDeviceAndroidClipboardPrivate;
	TArray<FString> Values;
	if (!CallNoArgMethod(
		"AndroidThunkJava_OpenMobileDeviceCheckClipboardContentTypes",
		Values
	) || Values.Num() != 4)
	{
		return MakeBridgeFailure();
	}
	FOpenMobileClipboardOperationResult Result;
	ApplyState(Values[0], Values[3], Result);
	if (Result.State == EOpenMobileClipboardOperationState::Succeeded
		|| Result.State == EOpenMobileClipboardOperationState::Empty)
	{
		Result.Content.bContentTypesAvailable = true;
		if (Values[1] == TEXT("true"))
		{
			Result.Content.ContentTypes.Add(
				EOpenMobileClipboardContentType::Text
			);
		}
		if (Values[2] == TEXT("true"))
		{
			Result.Content.ContentTypes.Add(
				EOpenMobileClipboardContentType::Url
			);
		}
		if (Result.Content.ContentTypes.IsEmpty()
			&& Result.State == EOpenMobileClipboardOperationState::Empty)
		{
			Result.Content.ContentTypes.Add(
				EOpenMobileClipboardContentType::Empty
			);
		}
		else if (Result.Content.ContentTypes.IsEmpty())
		{
			Result.Content.ContentTypes.Add(
				EOpenMobileClipboardContentType::Unknown
			);
		}
	}
	return Result;
}

FOpenMobileClipboardOperationResult WriteOpenMobileDeviceAndroidClipboard(
	const FOpenMobileClipboardWriteRequest& Request
)
{
	using namespace OpenMobileDeviceAndroidClipboardPrivate;
	TArray<FString> Values;
	if (!CallWriteMethod(Request, Values) || Values.Num() != 2)
	{
		return MakeBridgeFailure();
	}
	FOpenMobileClipboardOperationResult Result;
	ApplyState(Values[0], Values[1], Result);
	if (Result.State == EOpenMobileClipboardOperationState::Succeeded)
	{
		Result.Content.bContentTypesAvailable = true;
		Result.Content.ContentTypes.Add(Request.ContentType);
	}
	return Result;
}

FOpenMobileClipboardOperationResult ReadOpenMobileDeviceAndroidClipboard(
	EOpenMobileClipboardContentType ContentType
)
{
	using namespace OpenMobileDeviceAndroidClipboardPrivate;
	TArray<FString> Values;
	if (!CallReadMethod(ContentType, Values) || Values.Num() != 3)
	{
		return MakeBridgeFailure();
	}
	FOpenMobileClipboardOperationResult Result;
	ApplyState(Values[0], Values[2], Result);
	if (Result.State == EOpenMobileClipboardOperationState::Succeeded)
	{
		Result.Content.bContentTypesAvailable = true;
		Result.Content.ContentTypes.Add(ContentType);
		if (ContentType == EOpenMobileClipboardContentType::Text)
		{
			Result.Content.Text =
				FOpenMobileDeviceOptionalString::MakeAvailable(Values[1]);
		}
		else
		{
			Result.Content.Url =
				FOpenMobileDeviceOptionalString::MakeAvailable(Values[1]);
		}
	}
	return Result;
}

FOpenMobileClipboardOperationResult ClearOpenMobileDeviceAndroidClipboard()
{
	using namespace OpenMobileDeviceAndroidClipboardPrivate;
	TArray<FString> Values;
	if (!CallNoArgMethod(
		"AndroidThunkJava_OpenMobileDeviceClearClipboard",
		Values
	) || Values.Num() != 2)
	{
		return MakeBridgeFailure();
	}
	FOpenMobileClipboardOperationResult Result;
	ApplyState(Values[0], Values[1], Result);
	if (Result.IsSuccessful())
	{
		Result.Content.bContentTypesAvailable = true;
		Result.Content.ContentTypes.Add(EOpenMobileClipboardContentType::Empty);
	}
	return Result;
}

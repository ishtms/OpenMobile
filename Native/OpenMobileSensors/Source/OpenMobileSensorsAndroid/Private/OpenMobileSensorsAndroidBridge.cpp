#include "OpenMobileSensorsAndroidBridge.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileAsync.h"
#include "OpenMobileSensorsAndroidBackend.h"

namespace OpenMobileSensorsAndroidBridgePrivate
{
	constexpr int32 ResultOk = 0;
	constexpr int32 ResultInvalidArgument = -1;
	constexpr int32 ResultSensorMissing = -2;
	constexpr int32 ResultPermissionDenied = -3;
	constexpr int32 ResultRegisterFailed = -4;
	constexpr int32 ResultStreamMissing = -5;
	constexpr int32 ResultFlushFailed = -6;
	constexpr int32 ResultPaused = -7;
	constexpr int32 ResultShuttingDown = -8;
	constexpr int32 ResultTimeout = -9;
	constexpr int32 ResultActivityUnavailable = -10;
	constexpr int32 ResultPermissionNotDeclared = -11;
	constexpr int32 ResultBusy = -12;
	constexpr int32 PermissionStatusNotDetermined = 0;
	constexpr int32 PermissionStatusGranted = 1;
	constexpr int32 PermissionStatusDenied = 2;
	constexpr int32 PermissionStatusRestricted = 3;
	constexpr int32 PermissionStatusPermanentlyDenied = 4;
	constexpr int32 StringStride = 4;
	constexpr int32 IntegerStride = 10;
	constexpr int32 NumberStride = 3;
	constexpr int32 MaximumBatchSamples = 64;
	constexpr int32 MaximumValuesPerSample = 6;

	FCriticalSection ActiveBridgeMutex;
	FOpenMobileSensorsAndroidBridge* ActiveBridge = nullptr;

	bool TryMapPermissionStatus(
		int32 NativeStatus,
		EOpenMobilePermissionStatus& OutStatus
	)
	{
		switch (NativeStatus)
		{
		case PermissionStatusNotDetermined:
			OutStatus = EOpenMobilePermissionStatus::NotDetermined;
			return true;
		case PermissionStatusGranted:
			OutStatus = EOpenMobilePermissionStatus::Granted;
			return true;
		case PermissionStatusDenied:
			OutStatus = EOpenMobilePermissionStatus::Denied;
			return true;
		case PermissionStatusRestricted:
			OutStatus = EOpenMobilePermissionStatus::Restricted;
			return true;
		case PermissionStatusPermanentlyDenied:
			OutStatus = EOpenMobilePermissionStatus::PermanentlyDenied;
			return true;
		default:
			return false;
		}
	}

	FOpenMobileError PermissionErrorFromResult(int32 NativeResult)
	{
		switch (NativeResult)
		{
		case ResultActivityUnavailable:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The Android activity is unavailable."),
				TEXT("ActivityUnavailable")
			);
		case ResultPermissionNotDeclared:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::NotConfigured,
				TEXT("Android activity recognition is not declared in the manifest."),
				TEXT("PermissionNotDeclared")
			);
		case ResultBusy:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::Busy,
				TEXT("An Android activity-recognition prompt is already active."),
				TEXT("PermissionRequestBusy")
			);
		case ResultInvalidArgument:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The Android permission request is invalid."),
				TEXT("InvalidArgument")
			);
		case ResultShuttingDown:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The Android Sensors bridge is shutting down."),
				TEXT("ShuttingDown")
			);
		default:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The Android permission request failed."),
				FString::FromInt(NativeResult)
			);
		}
	}

	EOpenMobileSensorType MapNativeSensorType(int32 NativeType)
	{
		switch (NativeType)
		{
		case 1:
			return EOpenMobileSensorType::Accelerometer;
		case 35:
			return EOpenMobileSensorType::AccelerometerUncalibrated;
		case 4:
			return EOpenMobileSensorType::Gyroscope;
		case 16:
			return EOpenMobileSensorType::GyroscopeUncalibrated;
		case 2:
			return EOpenMobileSensorType::Magnetometer;
		case 14:
			return EOpenMobileSensorType::MagnetometerUncalibrated;
		case 9:
			return EOpenMobileSensorType::Gravity;
		case 10:
			return EOpenMobileSensorType::LinearAcceleration;
		case 11:
		case 15:
		case 20:
			return EOpenMobileSensorType::Attitude;
		case 42:
			return EOpenMobileSensorType::TrueHeading;
		case 6:
			return EOpenMobileSensorType::BarometricPressure;
		case 5:
			return EOpenMobileSensorType::AmbientLight;
		case 8:
			return EOpenMobileSensorType::Proximity;
		case 19:
			return EOpenMobileSensorType::StepCounter;
		case 18:
			return EOpenMobileSensorType::StepDetector;
		default:
			return EOpenMobileSensorType::Unknown;
		}
	}

	FGuid ParseGuid(JNIEnv* Env, jstring Value)
	{
		FGuid Parsed;
		if (Value)
		{
			FGuid::Parse(FJavaHelper::FStringFromParam(Env, Value), Parsed);
		}
		return Parsed;
	}

	void WithActiveBridge(
		TFunctionRef<void(FOpenMobileSensorsAndroidBridge&)> Callback
	)
	{
		FScopeLock Lock(&ActiveBridgeMutex);
		if (ActiveBridge)
		{
			Callback(*ActiveBridge);
		}
	}
}

JNI_METHOD void Java_com_openmobile_sensors_OpenMobileSensorsBridgeV1_nativeOnSampleBatch(
	JNIEnv* Env,
	jclass Class,
	jlong BackendGeneration,
	jstring StreamId,
	jint NativeSensorType,
	jint SampleCount,
	jint ValuesPerSample,
	jint ValueStride,
	jlongArray Timestamps,
	jfloatArray Values
)
{
	static_cast<void>(Class);
	using namespace OpenMobileSensorsAndroidBridgePrivate;
	const FGuid StreamIdentifier = ParseGuid(Env, StreamId);
	if (!StreamIdentifier.IsValid()
		|| SampleCount <= 0
		|| SampleCount > MaximumBatchSamples
		|| ValuesPerSample <= 0
		|| ValuesPerSample > MaximumValuesPerSample
		|| ValueStride < ValuesPerSample
		|| ValueStride > MaximumValuesPerSample
		|| !Timestamps
		|| !Values
		|| Env->GetArrayLength(Timestamps) < SampleCount
		|| Env->GetArrayLength(Values) < SampleCount * ValueStride
		|| Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
		}
		return;
	}
	TArray<int64> NativeTimestamps;
	NativeTimestamps.SetNumUninitialized(SampleCount);
	Env->GetLongArrayRegion(
		Timestamps,
		0,
		SampleCount,
		reinterpret_cast<jlong*>(NativeTimestamps.GetData())
	);
	TArray<float> NativeValues;
	NativeValues.SetNumUninitialized(SampleCount * ValueStride);
	Env->GetFloatArrayRegion(
		Values,
		0,
		SampleCount * ValueStride,
		NativeValues.GetData()
	);
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		return;
	}
	WithActiveBridge(
		[&](FOpenMobileSensorsAndroidBridge& Bridge)
		{
			Bridge.HandleSampleBatch(
				static_cast<uint64>(BackendGeneration),
				StreamIdentifier,
				NativeSensorType,
				SampleCount,
				ValuesPerSample,
				ValueStride,
				MoveTemp(NativeTimestamps),
				MoveTemp(NativeValues)
			);
		}
	);
}

JNI_METHOD void Java_com_openmobile_sensors_OpenMobileSensorsBridgeV1_nativeOnAccuracyChanged(
	JNIEnv* Env,
	jclass Class,
	jlong BackendGeneration,
	jstring StreamId,
	jint NativeSensorType,
	jint NativeAccuracy,
	jlong TimestampNanoseconds
)
{
	static_cast<void>(Class);
	const FGuid StreamIdentifier =
		OpenMobileSensorsAndroidBridgePrivate::ParseGuid(Env, StreamId);
	OpenMobileSensorsAndroidBridgePrivate::WithActiveBridge(
		[&](FOpenMobileSensorsAndroidBridge& Bridge)
		{
			Bridge.HandleAccuracyChanged(
				static_cast<uint64>(BackendGeneration),
				StreamIdentifier,
				NativeSensorType,
				NativeAccuracy,
				static_cast<int64>(TimestampNanoseconds)
			);
		}
	);
}

JNI_METHOD void Java_com_openmobile_sensors_OpenMobileSensorsBridgeV1_nativeOnFlushCompleted(
	JNIEnv* Env,
	jclass Class,
	jlong BackendGeneration,
	jstring StreamId,
	jstring RequestId,
	jint NativeResult
)
{
	static_cast<void>(Class);
	const FGuid StreamIdentifier =
		OpenMobileSensorsAndroidBridgePrivate::ParseGuid(Env, StreamId);
	const FGuid RequestIdentifier =
		OpenMobileSensorsAndroidBridgePrivate::ParseGuid(Env, RequestId);
	OpenMobileSensorsAndroidBridgePrivate::WithActiveBridge(
		[&](FOpenMobileSensorsAndroidBridge& Bridge)
		{
			Bridge.HandleFlushCompleted(
				static_cast<uint64>(BackendGeneration),
				StreamIdentifier,
				RequestIdentifier,
				NativeResult
			);
		}
	);
}

JNI_METHOD void Java_com_openmobile_sensors_OpenMobileSensorsBridgeV1_nativeOnSensorDisconnected(
	JNIEnv* Env,
	jclass Class,
	jlong BackendGeneration,
	jstring StreamId,
	jint NativeSensorType
)
{
	static_cast<void>(Class);
	const FGuid StreamIdentifier =
		OpenMobileSensorsAndroidBridgePrivate::ParseGuid(Env, StreamId);
	OpenMobileSensorsAndroidBridgePrivate::WithActiveBridge(
		[&](FOpenMobileSensorsAndroidBridge& Bridge)
		{
			Bridge.HandleSensorDisconnected(
				static_cast<uint64>(BackendGeneration),
				StreamIdentifier,
				NativeSensorType
			);
		}
	);
}

JNI_METHOD void Java_com_openmobile_sensors_OpenMobileSensorsBridgeV1_nativeOnStreamError(
	JNIEnv* Env,
	jclass Class,
	jlong BackendGeneration,
	jstring StreamId,
	jint NativeResult
)
{
	static_cast<void>(Class);
	const FGuid StreamIdentifier =
		OpenMobileSensorsAndroidBridgePrivate::ParseGuid(Env, StreamId);
	OpenMobileSensorsAndroidBridgePrivate::WithActiveBridge(
		[&](FOpenMobileSensorsAndroidBridge& Bridge)
		{
			Bridge.HandleStreamError(
				static_cast<uint64>(BackendGeneration),
				StreamIdentifier,
				NativeResult
			);
		}
	);
}

JNI_METHOD void Java_com_openmobile_sensors_OpenMobileSensorsBridgeV1_nativeOnStreamRestarted(
	JNIEnv* Env,
	jclass Class,
	jlong BackendGeneration,
	jstring StreamId
)
{
	static_cast<void>(Class);
	const FGuid StreamIdentifier =
		OpenMobileSensorsAndroidBridgePrivate::ParseGuid(Env, StreamId);
	OpenMobileSensorsAndroidBridgePrivate::WithActiveBridge(
		[&](FOpenMobileSensorsAndroidBridge& Bridge)
		{
			Bridge.HandleStreamRestarted(
				static_cast<uint64>(BackendGeneration),
				StreamIdentifier
			);
		}
	);
}

JNI_METHOD void Java_com_openmobile_sensors_OpenMobileSensorsBridgeV1_nativeOnSensorsChanged(
	JNIEnv* Env,
	jclass Class
)
{
	static_cast<void>(Env);
	static_cast<void>(Class);
	OpenMobileSensorsAndroidBridgePrivate::WithActiveBridge(
		[](FOpenMobileSensorsAndroidBridge& Bridge)
		{
			Bridge.HandleSensorsChanged();
		}
	);
}

JNI_METHOD void Java_com_openmobile_sensors_OpenMobileSensorsBridgeV1_nativeOnPermissionResult(
	JNIEnv* Env,
	jclass Class,
	jstring RequestIdentifier,
	jint NativeStatus
)
{
	static_cast<void>(Class);
	const FGuid ParsedRequest =
		OpenMobileSensorsAndroidBridgePrivate::ParseGuid(
			Env,
			RequestIdentifier
		);
	if (!ParsedRequest.IsValid())
	{
		return;
	}
	OpenMobileSensorsAndroidBridgePrivate::WithActiveBridge(
		[ParsedRequest, NativeStatus](
			FOpenMobileSensorsAndroidBridge& Bridge
		)
		{
			Bridge.HandlePermissionResult(
				ParsedRequest,
				static_cast<int32>(NativeStatus)
			);
		}
	);
}

FOpenMobileSensorsAndroidBridge::FOpenMobileSensorsAndroidBridge(
	FOpenMobileSensorsAndroidBackend& InBackend
)
	: Backend(InBackend)
{
	FScopeLock Lock(
		&OpenMobileSensorsAndroidBridgePrivate::ActiveBridgeMutex
	);
	OpenMobileSensorsAndroidBridgePrivate::ActiveBridge = this;
}

FOpenMobileSensorsAndroidBridge::~FOpenMobileSensorsAndroidBridge()
{
	Shutdown();
}

FOpenMobilePermissionResult FOpenMobileSensorsAndroidBridge::
QueryActivityRecognitionPermissionStatus()
{
	using namespace OpenMobileSensorsAndroidBridgePrivate;
	FOpenMobilePermissionResult Result;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		Result.Error = PermissionErrorFromResult(ResultActivityUnavailable);
		return Result;
	}
	jclass Bridge = FAndroidApplication::FindJavaClassGlobalRef(
		"com/openmobile/sensors/OpenMobileSensorsBridgeV1"
	);
	if (!Bridge)
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
		}
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android Sensors bridge class is unavailable."),
			TEXT("BridgeClassMissing")
		);
		return Result;
	}
	const jmethodID GetStatusMethod = Env->GetStaticMethodID(
		Bridge,
		"getActivityRecognitionPermissionStatus",
		"(Landroid/app/Activity;)I"
	);
	if (Env->ExceptionCheck() || !GetStatusMethod)
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
		}
		Env->DeleteGlobalRef(Bridge);
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The Android permission status bridge is unavailable."),
			TEXT("BridgeMethodMissing")
		);
		return Result;
	}
	const int32 NativeStatus = static_cast<int32>(
		Env->CallStaticIntMethod(Bridge, GetStatusMethod, Activity)
	);
	const bool bException = Env->ExceptionCheck();
	if (bException)
	{
		Env->ExceptionClear();
	}
	Env->DeleteGlobalRef(Bridge);
	if (bException)
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Android permission status lookup raised an exception."),
			TEXT("JavaException")
		);
		return Result;
	}
	if (!TryMapPermissionStatus(NativeStatus, Result.Status))
	{
		Result.Error = PermissionErrorFromResult(NativeStatus);
	}
	return Result;
}

bool FOpenMobileSensorsAndroidBridge::RequestActivityRecognitionPermission(
	const FGuid& RequestIdentifier,
	FOpenMobileNativePermissionCompletion&& Completion,
	FOpenMobileError& OutError
)
{
	using namespace OpenMobileSensorsAndroidBridgePrivate;
	if (!RequestIdentifier.IsValid() || !Completion.IsBound())
	{
		OutError = PermissionErrorFromResult(ResultInvalidArgument);
		return false;
	}
	const FOpenMobileSensorsAndroidBridgeResult Initialization =
		EnsureInitialized();
	if (!Initialization.IsSuccess())
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android Sensors bridge is unavailable."),
			FString::FromInt(static_cast<int32>(Initialization.Failure))
		);
		return false;
	}
	{
		FScopeLock Lock(&Mutex);
		if (PendingPermissionRequests.Contains(RequestIdentifier))
		{
			OutError = PermissionErrorFromResult(ResultInvalidArgument);
			return false;
		}
		PendingPermissionRequests.Add(
			RequestIdentifier,
			MoveTemp(Completion)
		);
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		FScopeLock Lock(&Mutex);
		PendingPermissionRequests.Remove(RequestIdentifier);
		OutError = PermissionErrorFromResult(ResultActivityUnavailable);
		return false;
	}
	const FScopedJavaObject<jstring> JavaRequestIdentifier =
		FJavaHelper::ToJavaString(Env, RequestIdentifier.ToString());
	const int32 NativeResult = static_cast<int32>(Env->CallIntMethod(
		static_cast<jobject>(BridgeObject),
		static_cast<jmethodID>(RequestActivityRecognitionPermissionMethod),
		*JavaRequestIdentifier
	));
	if (Env->ExceptionCheck() || NativeResult != ResultOk)
	{
		const bool bException = Env->ExceptionCheck();
		ClearException(Env);
		{
			FScopeLock Lock(&Mutex);
			PendingPermissionRequests.Remove(RequestIdentifier);
		}
		OutError = bException
			? FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("Android permission request raised an exception."),
				TEXT("JavaException")
			)
			: PermissionErrorFromResult(NativeResult);
		return false;
	}
	return true;
}

void FOpenMobileSensorsAndroidBridge::CancelPermissionRequest(
	const FGuid& RequestIdentifier
)
{
	{
		FScopeLock Lock(&Mutex);
		if (PendingPermissionRequests.Remove(RequestIdentifier) == 0)
		{
			return;
		}
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env || !BridgeObject || !CancelPermissionRequestMethod)
	{
		return;
	}
	const FScopedJavaObject<jstring> JavaRequestIdentifier =
		FJavaHelper::ToJavaString(Env, RequestIdentifier.ToString());
	Env->CallVoidMethod(
		static_cast<jobject>(BridgeObject),
		static_cast<jmethodID>(CancelPermissionRequestMethod),
		*JavaRequestIdentifier
	);
	ClearException(Env);
}

void FOpenMobileSensorsAndroidBridge::ClearException(
	void* Environment
) const
{
	JNIEnv* Env = static_cast<JNIEnv*>(Environment);
	if (Env && Env->ExceptionCheck())
	{
		Env->ExceptionClear();
	}
}

FOpenMobileSensorsAndroidBridgeResult
FOpenMobileSensorsAndroidBridge::EnsureInitialized()
{
	FScopeLock Lock(&Mutex);
	if (bShuttingDown.Load())
	{
		return {EOpenMobileSensorsAndroidBridgeFailure::ShuttingDown};
	}
	if (BridgeClass && BridgeObject)
	{
		return {};
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		return {EOpenMobileSensorsAndroidBridgeFailure::ActivityUnavailable};
	}
	jclass LocalBridgeClass =
		FAndroidApplication::FindJavaClassGlobalRef(
			"com/openmobile/sensors/OpenMobileSensorsBridgeV1"
		);
	if (!LocalBridgeClass)
	{
		ClearException(Env);
		return {EOpenMobileSensorsAndroidBridgeFailure::BridgeClassMissing};
	}
	const jmethodID LocalCreateMethod = Env->GetStaticMethodID(
		LocalBridgeClass,
		"create",
		"(Landroid/app/Activity;)Lcom/openmobile/sensors/OpenMobileSensorsBridgeV1;"
	);
	const jmethodID LocalQuerySensorsMethod = Env->GetMethodID(
		LocalBridgeClass,
		"querySensorSnapshot",
		"()[Ljava/lang/Object;"
	);
	const jmethodID LocalStartStreamMethod = Env->GetMethodID(
		LocalBridgeClass,
		"startStream",
		"(Ljava/lang/String;Ljava/lang/String;JIIZ)I"
	);
	const jmethodID LocalReconfigureStreamMethod = Env->GetMethodID(
		LocalBridgeClass,
		"reconfigureStream",
		"(Ljava/lang/String;IIZ)I"
	);
	const jmethodID LocalStopStreamMethod = Env->GetMethodID(
		LocalBridgeClass,
		"stopStream",
		"(Ljava/lang/String;)I"
	);
	const jmethodID LocalFlushStreamMethod = Env->GetMethodID(
		LocalBridgeClass,
		"flushStream",
		"(Ljava/lang/String;Ljava/lang/String;)I"
	);
	const jmethodID LocalHighSamplingMethod = Env->GetStaticMethodID(
		LocalBridgeClass,
		"hasHighSamplingRateDeclaration",
		"(Landroid/app/Activity;)Z"
	);
	const jmethodID LocalRequestPermissionMethod = Env->GetMethodID(
		LocalBridgeClass,
		"requestActivityRecognitionPermission",
		"(Ljava/lang/String;)I"
	);
	const jmethodID LocalCancelPermissionMethod = Env->GetMethodID(
		LocalBridgeClass,
		"cancelPermissionRequest",
		"(Ljava/lang/String;)V"
	);
	const jmethodID LocalShutdownMethod = Env->GetMethodID(
		LocalBridgeClass,
		"shutdown",
		"()V"
	);
	if (Env->ExceptionCheck()
		|| !LocalCreateMethod
		|| !LocalQuerySensorsMethod
		|| !LocalStartStreamMethod
		|| !LocalReconfigureStreamMethod
		|| !LocalStopStreamMethod
		|| !LocalFlushStreamMethod
		|| !LocalHighSamplingMethod
		|| !LocalRequestPermissionMethod
		|| !LocalCancelPermissionMethod
		|| !LocalShutdownMethod)
	{
		ClearException(Env);
		Env->DeleteGlobalRef(LocalBridgeClass);
		return {EOpenMobileSensorsAndroidBridgeFailure::BridgeMethodMissing};
	}
	jobject LocalBridgeObject = Env->CallStaticObjectMethod(
		LocalBridgeClass,
		LocalCreateMethod,
		Activity
	);
	if (Env->ExceptionCheck() || !LocalBridgeObject)
	{
		ClearException(Env);
		Env->DeleteGlobalRef(LocalBridgeClass);
		return {EOpenMobileSensorsAndroidBridgeFailure::BridgeCreateFailed};
	}
	jobject GlobalBridgeObject = Env->NewGlobalRef(LocalBridgeObject);
	if (Env->ExceptionCheck() || !GlobalBridgeObject)
	{
		ClearException(Env);
		Env->CallVoidMethod(LocalBridgeObject, LocalShutdownMethod);
		ClearException(Env);
		Env->DeleteLocalRef(LocalBridgeObject);
		Env->DeleteGlobalRef(LocalBridgeClass);
		return {EOpenMobileSensorsAndroidBridgeFailure::BridgeCreateFailed};
	}
	Env->DeleteLocalRef(LocalBridgeObject);
	BridgeClass = LocalBridgeClass;
	BridgeObject = GlobalBridgeObject;
	CreateMethod = LocalCreateMethod;
	QuerySensorsMethod = LocalQuerySensorsMethod;
	StartStreamMethod = LocalStartStreamMethod;
	ReconfigureStreamMethod = LocalReconfigureStreamMethod;
	StopStreamMethod = LocalStopStreamMethod;
	FlushStreamMethod = LocalFlushStreamMethod;
	HasHighSamplingRateDeclarationMethod = LocalHighSamplingMethod;
	RequestActivityRecognitionPermissionMethod =
		LocalRequestPermissionMethod;
	CancelPermissionRequestMethod = LocalCancelPermissionMethod;
	ShutdownMethod = LocalShutdownMethod;
	return {};
}

FOpenMobileSensorsAndroidBridgeResult
FOpenMobileSensorsAndroidBridge::QuerySensors(
	TArray<FOpenMobileSensorsAndroidSensorDescriptor>& OutSensors
)
{
	using namespace OpenMobileSensorsAndroidBridgePrivate;
	OutSensors.Reset();
	const FOpenMobileSensorsAndroidBridgeResult Initialization =
		EnsureInitialized();
	if (!Initialization.IsSuccess())
	{
		return Initialization;
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FScopedJavaObject<jobjectArray> Snapshot(static_cast<jobjectArray>(
		Env->CallObjectMethod(
			static_cast<jobject>(BridgeObject),
			static_cast<jmethodID>(QuerySensorsMethod)
		)
	));
	if (Env->ExceptionCheck() || !Snapshot
		|| Env->GetArrayLength(*Snapshot) != 3)
	{
		ClearException(Env);
		return {EOpenMobileSensorsAndroidBridgeFailure::InvalidPayload};
	}
	FScopedJavaObject<jobjectArray> Strings(static_cast<jobjectArray>(
		Env->GetObjectArrayElement(*Snapshot, 0)
	));
	FScopedJavaObject<jlongArray> Integers(static_cast<jlongArray>(
		Env->GetObjectArrayElement(*Snapshot, 1)
	));
	FScopedJavaObject<jfloatArray> Numbers(static_cast<jfloatArray>(
		Env->GetObjectArrayElement(*Snapshot, 2)
	));
	if (Env->ExceptionCheck() || !Strings || !Integers || !Numbers)
	{
		ClearException(Env);
		return {EOpenMobileSensorsAndroidBridgeFailure::InvalidPayload};
	}
	const int32 StringCount = Env->GetArrayLength(*Strings);
	const int32 IntegerCount = Env->GetArrayLength(*Integers);
	const int32 NumberCount = Env->GetArrayLength(*Numbers);
	if (Env->ExceptionCheck()
		|| StringCount % StringStride != 0
		|| IntegerCount % IntegerStride != 0
		|| NumberCount % NumberStride != 0
		|| StringCount / StringStride != IntegerCount / IntegerStride
		|| StringCount / StringStride != NumberCount / NumberStride)
	{
		ClearException(Env);
		return {EOpenMobileSensorsAndroidBridgeFailure::InvalidPayload};
	}
	TArray<int64> NativeIntegers;
	NativeIntegers.SetNumUninitialized(IntegerCount);
	if (IntegerCount > 0)
	{
		Env->GetLongArrayRegion(
			*Integers,
			0,
			IntegerCount,
			reinterpret_cast<jlong*>(NativeIntegers.GetData())
		);
	}
	TArray<float> NativeNumbers;
	NativeNumbers.SetNumUninitialized(NumberCount);
	if (NumberCount > 0)
	{
		Env->GetFloatArrayRegion(
			*Numbers,
			0,
			NumberCount,
			NativeNumbers.GetData()
		);
	}
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return {EOpenMobileSensorsAndroidBridgeFailure::InvalidPayload};
	}
	const int32 SensorCount = StringCount / StringStride;
	OutSensors.Reserve(SensorCount);
	for (int32 Index = 0; Index < SensorCount; ++Index)
	{
		const int32 IntegerOffset = Index * IntegerStride;
		const int32 NumberOffset = Index * NumberStride;
		FOpenMobileSensorsAndroidSensorDescriptor Descriptor;
		Descriptor.NativeType = static_cast<int32>(
			NativeIntegers[IntegerOffset]
		);
		Descriptor.Sensor.Type = MapNativeSensorType(Descriptor.NativeType);
		if (Descriptor.Sensor.Type == EOpenMobileSensorType::Unknown)
		{
			continue;
		}
		auto ReadString = [Env, &Strings, Index](int32 Field)
		{
			return FJavaHelper::FStringFromLocalRef(
				Env,
				static_cast<jstring>(Env->GetObjectArrayElement(
					*Strings,
					Index * StringStride + Field
				))
			);
		};
		Descriptor.NativeIdentifier = ReadString(0);
		Descriptor.Sensor.InstanceId = FName(
			*Descriptor.NativeIdentifier
		);
		Descriptor.NativeName = ReadString(1);
		Descriptor.Vendor = ReadString(2);
		Descriptor.NativeStringType = ReadString(3);
		Descriptor.NativeId = static_cast<int32>(
			NativeIntegers[IntegerOffset + 1]
		);
		Descriptor.Version = static_cast<int32>(
			NativeIntegers[IntegerOffset + 2]
		);
		Descriptor.MinimumDelayMicroseconds = static_cast<int32>(
			NativeIntegers[IntegerOffset + 3]
		);
		Descriptor.MaximumDelayMicroseconds = static_cast<int32>(
			NativeIntegers[IntegerOffset + 4]
		);
		Descriptor.FifoCapacitySamples = static_cast<int32>(
			NativeIntegers[IntegerOffset + 5]
		);
		Descriptor.NativeReportingMode = static_cast<int32>(
			NativeIntegers[IntegerOffset + 6]
		);
		Descriptor.bWakeUp = NativeIntegers[IntegerOffset + 7] != 0;
		Descriptor.bPreferred = NativeIntegers[IntegerOffset + 8] != 0;
		Descriptor.bDynamic = NativeIntegers[IntegerOffset + 9] != 0;
		Descriptor.MaximumRange = NativeNumbers[NumberOffset];
		Descriptor.Resolution = NativeNumbers[NumberOffset + 1];
		Descriptor.PowerMilliwatts = NativeNumbers[NumberOffset + 2];
		OutSensors.Add(MoveTemp(Descriptor));
	}
	return {};
}

FOpenMobileSensorsAndroidBridgeResult
FOpenMobileSensorsAndroidBridge::StartStream(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor,
	int32 SamplingPeriodMicroseconds,
	int32 MaximumReportLatencyMicroseconds,
	bool bLowLatency,
	EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame
)
{
	if (Token.Generation == 0
		|| !Handle.IsValid()
		|| Descriptor.NativeIdentifier.IsEmpty()
		|| SamplingPeriodMicroseconds <= 0
		|| MaximumReportLatencyMicroseconds < 0)
	{
		return {EOpenMobileSensorsAndroidBridgeFailure::InvalidArgument};
	}
	const FOpenMobileSensorsAndroidBridgeResult Initialization =
		EnsureInitialized();
	if (!Initialization.IsSuccess())
	{
		return Initialization;
	}
	{
		FScopeLock Lock(&Mutex);
		if (ActiveStreams.Contains(Handle.Identifier))
		{
			return {EOpenMobileSensorsAndroidBridgeFailure::InvalidArgument};
		}
		FActiveStream Active;
		Active.Token = Token;
		Active.Handle = Handle;
		Active.Descriptor = Descriptor;
		Active.AttitudeReferenceFrame = AttitudeReferenceFrame;
		ActiveStreams.Add(Handle.Identifier, MoveTemp(Active));
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FScopedJavaObject<jstring> JavaStreamId = FJavaHelper::ToJavaString(
		Env,
		StreamId(Handle)
	);
	FScopedJavaObject<jstring> JavaNativeIdentifier =
		FJavaHelper::ToJavaString(Env, Descriptor.NativeIdentifier);
	const int32 NativeResult = Env->CallIntMethod(
		static_cast<jobject>(BridgeObject),
		static_cast<jmethodID>(StartStreamMethod),
		*JavaStreamId,
		*JavaNativeIdentifier,
		static_cast<jlong>(Token.Generation),
		SamplingPeriodMicroseconds,
		MaximumReportLatencyMicroseconds,
		bLowLatency ? JNI_TRUE : JNI_FALSE
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		FScopeLock Lock(&Mutex);
		ActiveStreams.Remove(Handle.Identifier);
		return {EOpenMobileSensorsAndroidBridgeFailure::JavaException};
	}
	const FOpenMobileSensorsAndroidBridgeResult Result =
		MapNativeResult(NativeResult);
	if (!Result.IsSuccess())
	{
		FScopeLock Lock(&Mutex);
		ActiveStreams.Remove(Handle.Identifier);
	}
	return Result;
}

FOpenMobileSensorsAndroidBridgeResult
FOpenMobileSensorsAndroidBridge::ReconfigureStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	int32 SamplingPeriodMicroseconds,
	int32 MaximumReportLatencyMicroseconds,
	bool bLowLatency,
	EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame
)
{
	const FOpenMobileSensorsAndroidBridgeResult Initialization =
		EnsureInitialized();
	if (!Initialization.IsSuccess())
	{
		return Initialization;
	}
	{
		FScopeLock Lock(&Mutex);
		if (!ActiveStreams.Contains(Handle.Identifier))
		{
			return {EOpenMobileSensorsAndroidBridgeFailure::StreamMissing};
		}
		for (auto Iterator = PendingFlushes.CreateIterator(); Iterator; ++Iterator)
		{
			if (Iterator.Value().Handle == Handle)
			{
				Iterator.RemoveCurrent();
			}
		}
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FScopedJavaObject<jstring> JavaStreamId = FJavaHelper::ToJavaString(
		Env,
		StreamId(Handle)
	);
	const int32 NativeResult = Env->CallIntMethod(
		static_cast<jobject>(BridgeObject),
		static_cast<jmethodID>(ReconfigureStreamMethod),
		*JavaStreamId,
		SamplingPeriodMicroseconds,
		MaximumReportLatencyMicroseconds,
		bLowLatency ? JNI_TRUE : JNI_FALSE
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return {EOpenMobileSensorsAndroidBridgeFailure::JavaException};
	}
	const FOpenMobileSensorsAndroidBridgeResult Result =
		MapNativeResult(NativeResult);
	if (Result.IsSuccess())
	{
		FScopeLock Lock(&Mutex);
		if (FActiveStream* Active = ActiveStreams.Find(Handle.Identifier))
		{
			Active->AttitudeReferenceFrame = AttitudeReferenceFrame;
		}
	}
	return Result;
}

bool FOpenMobileSensorsAndroidBridge::GetActiveSensorDescriptor(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorsAndroidSensorDescriptor& OutDescriptor
)
{
	FScopeLock Lock(&Mutex);
	const FActiveStream* Active = ActiveStreams.Find(Handle.Identifier);
	if (!Active)
	{
		return false;
	}
	OutDescriptor = Active->Descriptor;
	return true;
}

FOpenMobileSensorsAndroidBridgeResult
FOpenMobileSensorsAndroidBridge::StopStream(
	const FOpenMobileSensorBackendStreamHandle& Handle
)
{
	const FOpenMobileSensorsAndroidBridgeResult Initialization =
		EnsureInitialized();
	if (!Initialization.IsSuccess())
	{
		return Initialization;
	}
	{
		FScopeLock Lock(&Mutex);
		ActiveStreams.Remove(Handle.Identifier);
		for (auto Iterator = PendingFlushes.CreateIterator(); Iterator; ++Iterator)
		{
			if (Iterator.Value().Handle == Handle)
			{
				Iterator.RemoveCurrent();
			}
		}
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FScopedJavaObject<jstring> JavaStreamId = FJavaHelper::ToJavaString(
		Env,
		StreamId(Handle)
	);
	const int32 NativeResult = Env->CallIntMethod(
		static_cast<jobject>(BridgeObject),
		static_cast<jmethodID>(StopStreamMethod),
		*JavaStreamId
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return {EOpenMobileSensorsAndroidBridgeFailure::JavaException};
	}
	return MapNativeResult(NativeResult);
}

FOpenMobileSensorsAndroidBridgeResult
FOpenMobileSensorsAndroidBridge::FlushStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FGuid& RequestId,
	FOnOpenMobileSensorBackendFlushComplete&& Completion
)
{
	const FOpenMobileSensorsAndroidBridgeResult Initialization =
		EnsureInitialized();
	if (!Initialization.IsSuccess())
	{
		return Initialization;
	}
	{
		FScopeLock Lock(&Mutex);
		const FActiveStream* Active = ActiveStreams.Find(Handle.Identifier);
		if (!Active)
		{
			return {EOpenMobileSensorsAndroidBridgeFailure::StreamMissing};
		}
		if (!RequestId.IsValid() || PendingFlushes.Contains(RequestId))
		{
			return {EOpenMobileSensorsAndroidBridgeFailure::InvalidArgument};
		}
		FPendingFlush Pending;
		Pending.Token = Active->Token;
		Pending.Handle = Handle;
		Pending.Completion = MoveTemp(Completion);
		PendingFlushes.Add(RequestId, MoveTemp(Pending));
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FScopedJavaObject<jstring> JavaStreamId = FJavaHelper::ToJavaString(
		Env,
		StreamId(Handle)
	);
	FScopedJavaObject<jstring> JavaRequestId = FJavaHelper::ToJavaString(
		Env,
		RequestId.ToString(EGuidFormats::DigitsWithHyphens)
	);
	const int32 NativeResult = Env->CallIntMethod(
		static_cast<jobject>(BridgeObject),
		static_cast<jmethodID>(FlushStreamMethod),
		*JavaStreamId,
		*JavaRequestId
	);
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		FScopeLock Lock(&Mutex);
		PendingFlushes.Remove(RequestId);
		return {EOpenMobileSensorsAndroidBridgeFailure::JavaException};
	}
	const FOpenMobileSensorsAndroidBridgeResult Result =
		MapNativeResult(NativeResult);
	if (!Result.IsSuccess())
	{
		FScopeLock Lock(&Mutex);
		PendingFlushes.Remove(RequestId);
	}
	return Result;
}

bool FOpenMobileSensorsAndroidBridge::HasHighSamplingRateDeclaration()
{
	const FOpenMobileSensorsAndroidBridgeResult Initialization =
		EnsureInitialized();
	if (!Initialization.IsSuccess())
	{
		return false;
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		return false;
	}
	const bool bDeclared = Env->CallStaticBooleanMethod(
		static_cast<jclass>(BridgeClass),
		static_cast<jmethodID>(HasHighSamplingRateDeclarationMethod),
		Activity
	) == JNI_TRUE;
	if (Env->ExceptionCheck())
	{
		ClearException(Env);
		return false;
	}
	return bDeclared;
}

void FOpenMobileSensorsAndroidBridge::Shutdown()
{
	if (bShuttingDown.Exchange(true))
	{
		return;
	}
	TArray<FOpenMobileNativePermissionCompletion>
		PendingPermissionCompletions;
	{
		FScopeLock Lock(
			&OpenMobileSensorsAndroidBridgePrivate::ActiveBridgeMutex
		);
		if (OpenMobileSensorsAndroidBridgePrivate::ActiveBridge == this)
		{
			OpenMobileSensorsAndroidBridgePrivate::ActiveBridge = nullptr;
		}
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env && BridgeObject && ShutdownMethod)
	{
		Env->CallVoidMethod(
			static_cast<jobject>(BridgeObject),
			static_cast<jmethodID>(ShutdownMethod)
		);
		ClearException(Env);
	}
	{
		FScopeLock Lock(&Mutex);
		ActiveStreams.Reset();
		PendingFlushes.Reset();
		PendingPermissionCompletions.Reserve(
			PendingPermissionRequests.Num()
		);
		for (TPair<FGuid, FOpenMobileNativePermissionCompletion>& Pair
			: PendingPermissionRequests)
		{
			PendingPermissionCompletions.Add(MoveTemp(Pair.Value));
		}
		PendingPermissionRequests.Reset();
	}
	if (Env && BridgeObject)
	{
		Env->DeleteGlobalRef(static_cast<jobject>(BridgeObject));
	}
	if (Env && BridgeClass)
	{
		Env->DeleteGlobalRef(static_cast<jclass>(BridgeClass));
	}
	BridgeClass = nullptr;
	BridgeObject = nullptr;
	CreateMethod = nullptr;
	QuerySensorsMethod = nullptr;
	StartStreamMethod = nullptr;
	ReconfigureStreamMethod = nullptr;
	StopStreamMethod = nullptr;
	FlushStreamMethod = nullptr;
	HasHighSamplingRateDeclarationMethod = nullptr;
	RequestActivityRecognitionPermissionMethod = nullptr;
	CancelPermissionRequestMethod = nullptr;
	ShutdownMethod = nullptr;
	const FOpenMobileError PermissionShutdownError = FOpenMobileError::Make(
		EOpenMobileErrorCode::Unavailable,
		TEXT("The Android sensor permission service is shutting down.")
	);
	for (FOpenMobileNativePermissionCompletion& Completion
		: PendingPermissionCompletions)
	{
		Completion.ExecuteIfBound(
			EOpenMobilePermissionStatus::NotDetermined,
			PermissionShutdownError
		);
	}
}

FOpenMobileSensorsAndroidBridgeResult
FOpenMobileSensorsAndroidBridge::MapNativeResult(int32 NativeResult) const
{
	using namespace OpenMobileSensorsAndroidBridgePrivate;
	switch (NativeResult)
	{
	case ResultOk:
		return {};
	case ResultInvalidArgument:
		return {EOpenMobileSensorsAndroidBridgeFailure::InvalidArgument,
			NativeResult};
	case ResultSensorMissing:
		return {EOpenMobileSensorsAndroidBridgeFailure::SensorMissing,
			NativeResult};
	case ResultPermissionDenied:
		return {EOpenMobileSensorsAndroidBridgeFailure::PermissionDenied,
			NativeResult};
	case ResultRegisterFailed:
		return {EOpenMobileSensorsAndroidBridgeFailure::RegisterFailed,
			NativeResult};
	case ResultStreamMissing:
		return {EOpenMobileSensorsAndroidBridgeFailure::StreamMissing,
			NativeResult};
	case ResultFlushFailed:
		return {EOpenMobileSensorsAndroidBridgeFailure::FlushFailed,
			NativeResult};
	case ResultPaused:
		return {EOpenMobileSensorsAndroidBridgeFailure::Paused, NativeResult};
	case ResultShuttingDown:
		return {EOpenMobileSensorsAndroidBridgeFailure::ShuttingDown,
			NativeResult};
	case ResultTimeout:
		return {EOpenMobileSensorsAndroidBridgeFailure::Timeout, NativeResult};
	default:
		return {EOpenMobileSensorsAndroidBridgeFailure::JavaException,
			NativeResult};
	}
}

FString FOpenMobileSensorsAndroidBridge::StreamId(
	const FOpenMobileSensorBackendStreamHandle& Handle
) const
{
	return Handle.Identifier.ToString(EGuidFormats::DigitsWithHyphens);
}

bool FOpenMobileSensorsAndroidBridge::FindActiveStream(
	uint64 BackendGeneration,
	const FGuid& StreamIdentifier,
	FActiveStream& OutStream,
	bool bConsumeReset
)
{
	FScopeLock Lock(&Mutex);
	FActiveStream* Active = ActiveStreams.Find(StreamIdentifier);
	if (!Active
		|| Active->Token.Generation != BackendGeneration
		|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Active->Token))
	{
		return false;
	}
	OutStream = *Active;
	if (bConsumeReset && Active->bResetNextSample)
	{
		OutStream.bResetNextSample = true;
		Active->bResetNextSample = false;
	}
	return true;
}

void FOpenMobileSensorsAndroidBridge::HandleSampleBatch(
	uint64 BackendGeneration,
	const FGuid& StreamIdentifier,
	int32 NativeSensorType,
	int32 SampleCount,
	int32 ValuesPerSample,
	int32 ValueStride,
	TArray<int64>&& TimestampsNanoseconds,
	TArray<float>&& Values
)
{
	FActiveStream Active;
	if (!FindActiveStream(
		BackendGeneration,
		StreamIdentifier,
		Active,
		true
	) || Active.Descriptor.NativeType != NativeSensorType)
	{
		return;
	}
	Backend.PublishCompactBatchFromHandler(
		Active.Token,
		Active.Handle,
		Active.Descriptor,
		SampleCount,
		ValuesPerSample,
		ValueStride,
		MoveTemp(TimestampsNanoseconds),
		MoveTemp(Values),
		Active.bResetNextSample,
		Active.AttitudeReferenceFrame
	);
}

void FOpenMobileSensorsAndroidBridge::HandleAccuracyChanged(
	uint64 BackendGeneration,
	const FGuid& StreamIdentifier,
	int32 NativeSensorType,
	int32 NativeAccuracy,
	int64 TimestampNanoseconds
)
{
	FActiveStream Active;
	if (!FindActiveStream(
		BackendGeneration,
		StreamIdentifier,
		Active,
		false
	) || Active.Descriptor.NativeType != NativeSensorType)
	{
		return;
	}
	Backend.PublishAccuracyFromHandler(
		Active.Token,
		Active.Handle,
		Active.Descriptor.Sensor,
		NativeAccuracy,
		Backend.ConvertSensorEventTimestampNanoseconds(TimestampNanoseconds)
	);
}

void FOpenMobileSensorsAndroidBridge::HandleFlushCompleted(
	uint64 BackendGeneration,
	const FGuid& StreamIdentifier,
	const FGuid& RequestId,
	int32 NativeResult
)
{
	FPendingFlush Pending;
	{
		FScopeLock Lock(&Mutex);
		const FActiveStream* Active = ActiveStreams.Find(StreamIdentifier);
		FPendingFlush* Found = PendingFlushes.Find(RequestId);
		if (!Active
			|| !Found
			|| Active->Token.Generation != BackendGeneration
			|| Found->Token.Generation != BackendGeneration
			|| Found->Handle.Identifier != StreamIdentifier
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Found->Token))
		{
			return;
		}
		Pending = MoveTemp(*Found);
		PendingFlushes.Remove(RequestId);
	}
	if (Pending.Completion.IsBound())
	{
		const FOpenMobileSensorsAndroidBridgeResult Result =
			MapNativeResult(NativeResult);
		FOpenMobileSensorOperationResult Operation;
		if (Result.IsSuccess())
		{
			Operation.Code = EOpenMobileSensorResultCode::Success;
		}
		else
		{
			Operation = Backend.MapBridgeFailure(
				Result.Failure
			);
		}
		Pending.Completion.Execute(RequestId, Operation);
	}
}

void FOpenMobileSensorsAndroidBridge::HandleSensorDisconnected(
	uint64 BackendGeneration,
	const FGuid& StreamIdentifier,
	int32 NativeSensorType
)
{
	FActiveStream Active;
	if (!FindActiveStream(
		BackendGeneration,
		StreamIdentifier,
		Active,
		false
	) || Active.Descriptor.NativeType != NativeSensorType)
	{
		return;
	}
	{
		FScopeLock Lock(&Mutex);
		ActiveStreams.Remove(StreamIdentifier);
		for (auto Iterator = PendingFlushes.CreateIterator(); Iterator; ++Iterator)
		{
			if (Iterator.Value().Handle == Active.Handle)
			{
				Iterator.RemoveCurrent();
			}
		}
	}
	Backend.HandlePhysicalStreamFailureFromHandler(
		Active.Token,
		Active.Handle,
		EOpenMobileSensorsAndroidBridgeFailure::SensorMissing,
		TEXT("SensorDisconnected")
	);
}

void FOpenMobileSensorsAndroidBridge::HandleStreamError(
	uint64 BackendGeneration,
	const FGuid& StreamIdentifier,
	int32 NativeResult
)
{
	FActiveStream Active;
	if (!FindActiveStream(
		BackendGeneration,
		StreamIdentifier,
		Active,
		false
	))
	{
		return;
	}
	{
		FScopeLock Lock(&Mutex);
		ActiveStreams.Remove(StreamIdentifier);
		for (auto Iterator = PendingFlushes.CreateIterator(); Iterator; ++Iterator)
		{
			if (Iterator.Value().Handle == Active.Handle)
			{
				Iterator.RemoveCurrent();
			}
		}
	}
	const FOpenMobileSensorsAndroidBridgeResult Result =
		MapNativeResult(NativeResult);
	Backend.HandlePhysicalStreamFailureFromHandler(
		Active.Token,
		Active.Handle,
		Result.Failure,
		{}
	);
}

void FOpenMobileSensorsAndroidBridge::HandleStreamRestarted(
	uint64 BackendGeneration,
	const FGuid& StreamIdentifier
)
{
	FScopeLock Lock(&Mutex);
	FActiveStream* Active = ActiveStreams.Find(StreamIdentifier);
	if (Active && Active->Token.Generation == BackendGeneration)
	{
		Active->bResetNextSample = true;
	}
}

void FOpenMobileSensorsAndroidBridge::HandleSensorsChanged()
{
	Backend.HandleSensorsChangedFromHandler();
}

void FOpenMobileSensorsAndroidBridge::HandlePermissionResult(
	const FGuid& RequestIdentifier,
	int32 NativeStatus
)
{
	using namespace OpenMobileSensorsAndroidBridgePrivate;
	FOpenMobileNativePermissionCompletion Completion;
	{
		FScopeLock Lock(&Mutex);
		FOpenMobileNativePermissionCompletion* Found =
			PendingPermissionRequests.Find(RequestIdentifier);
		if (!Found)
		{
			return;
		}
		Completion = MoveTemp(*Found);
		PendingPermissionRequests.Remove(RequestIdentifier);
	}
	EOpenMobilePermissionStatus Status =
		EOpenMobilePermissionStatus::NotDetermined;
	FOpenMobileError Error;
	if (!TryMapPermissionStatus(NativeStatus, Status))
	{
		Error = PermissionErrorFromResult(NativeStatus);
	}
	OpenMobile::DispatchToGameThread(
		[Completion = MoveTemp(Completion), Status,
			Error = MoveTemp(Error)]() mutable
		{
			Completion.ExecuteIfBound(Status, MoveTemp(Error));
		}
	);
}

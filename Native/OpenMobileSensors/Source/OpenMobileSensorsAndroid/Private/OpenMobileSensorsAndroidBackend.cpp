#include "OpenMobileSensorsAndroidBackend.h"

#include "Misc/ScopeLock.h"
#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorTimestamp.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsSampleService.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

FOpenMobileSensorsAndroidBackend::~FOpenMobileSensorsAndroidBackend()
{
	StopSensorHandlerThread();
}

FName FOpenMobileSensorsAndroidBackend::GetBackendName() const
{
	return TEXT("Android");
}

double FOpenMobileSensorsAndroidBackend::
ConvertSensorEventTimestampNanoseconds(int64 TimestampNanoseconds)
{
	return FOpenMobileSensorTimestampConverter::
		FromAndroidSensorEventNanoseconds(TimestampNanoseconds);
}

bool FOpenMobileSensorsAndroidBackend::
CaptureApplicationWindowRotationFromUIThread(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorScreenRotation Rotation,
	double TimestampSeconds,
	bool bNaturalOrientationLandscape
)
{
	return FOpenMobileSensorsScreenRotationService::
		CaptureApplicationWindowRotation(
			OwnerIdentifier,
			Rotation,
			TimestampSeconds,
			bNaturalOrientationLandscape
		);
}

FOpenMobileCapability
FOpenMobileSensorsAndroidBackend::GetBackendCapability() const
{
	FOpenMobileCapability Capability;
	Capability.Name = GetModularFeatureName();
	Capability.State = EOpenMobileCapabilityState::Available;
	Capability.Detail = TEXT("The Android Sensors backend is registered.");
	return Capability;
}

bool FOpenMobileSensorsAndroidBackend::
RequiresHighSamplingRateDeclaration() const
{
	return true;
}

bool FOpenMobileSensorsAndroidBackend::HasHighSamplingRateDeclaration() const
{
#if PLATFORM_ANDROID
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
			"AndroidThunkJava_OpenMobileSensorsHasHighSamplingRateDeclaration",
			"()Z"
		)
		: nullptr;
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		return false;
	}
	if (!Method)
	{
		return false;
	}
	const bool bDeclared = Env->CallBooleanMethod(Activity, Method) == JNI_TRUE;
	if (Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		return false;
	}
	return bDeclared;
#else
	return true;
#endif
}

bool FOpenMobileSensorsAndroidBackend::PublishVectorBatchFromHandler(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileVectorSensorBatch& Batch
)
{
	if (!EnsureSensorHandlerThread())
	{
		return false;
	}
	FOpenMobileVectorSensorBatch NormalizedBatch = Batch;
	for (FOpenMobileVectorSensorSample& Sample : NormalizedBatch.Samples)
	{
		if (Sample.Header.SourceFlags == 0)
		{
			Sample.Header.SourceFlags =
				FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
					Sample.Header.Sensor.Type
				);
		}
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(
			EOpenMobileSensorNativePlatform::Android,
			Sample
		);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			EOpenMobileSensorNativePlatform::Android,
			Sample
		);
	}
	return FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		Handle,
		NormalizedBatch
	);
}

bool FOpenMobileSensorsAndroidBackend::PublishAccuracyFromHandler(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorIdentifier& Sensor,
	int32 NativeAccuracy,
	double TimestampSeconds
)
{
	if (!EnsureSensorHandlerThread())
	{
		return false;
	}
	return FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
		Token,
		Handle,
		FOpenMobileSensorAccuracyMapper::FromAndroidAccuracyCallback(
			Sensor,
			NativeAccuracy,
			TimestampSeconds
		)
	);
}

void FOpenMobileSensorsAndroidBackend::BeginShutdown()
{
	bShuttingDown.Store(true);
	StopSensorHandlerThread();
}

bool FOpenMobileSensorsAndroidBackend::EnsureSensorHandlerThread()
{
#if PLATFORM_ANDROID
	FScopeLock Lock(&HandlerMutex);
	if (bShuttingDown.Load())
	{
		return false;
	}
	if (SensorHandlerThread && SensorHandler)
	{
		return true;
	}
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		return false;
	}
	jclass HandlerThreadClass = FAndroidApplication::FindJavaClass(
		"android/os/HandlerThread"
	);
	jclass HandlerClass = FAndroidApplication::FindJavaClass(
		"android/os/Handler"
	);
	if (!HandlerThreadClass || !HandlerClass)
	{
		return false;
	}
	const jmethodID ThreadConstructor = Env->GetMethodID(
		HandlerThreadClass,
		"<init>",
		"(Ljava/lang/String;)V"
	);
	const jmethodID StartMethod = Env->GetMethodID(
		HandlerThreadClass,
		"start",
		"()V"
	);
	const jmethodID GetLooperMethod = Env->GetMethodID(
		HandlerThreadClass,
		"getLooper",
		"()Landroid/os/Looper;"
	);
	const jmethodID HandlerConstructor = Env->GetMethodID(
		HandlerClass,
		"<init>",
		"(Landroid/os/Looper;)V"
	);
	if (!ThreadConstructor || !StartMethod || !GetLooperMethod
		|| !HandlerConstructor)
	{
		return false;
	}
	jstring ThreadName = Env->NewStringUTF("OpenMobileSensorsHandler");
	jobject LocalThread = Env->NewObject(
		HandlerThreadClass,
		ThreadConstructor,
		ThreadName
	);
	Env->DeleteLocalRef(ThreadName);
	if (!LocalThread)
	{
		return false;
	}
	Env->CallVoidMethod(LocalThread, StartMethod);
	jobject Looper = Env->CallObjectMethod(LocalThread, GetLooperMethod);
	jobject LocalHandler = Looper
		? Env->NewObject(HandlerClass, HandlerConstructor, Looper)
		: nullptr;
	if (Looper)
	{
		Env->DeleteLocalRef(Looper);
	}
	if (!LocalHandler || Env->ExceptionCheck())
	{
		Env->ExceptionClear();
		if (LocalHandler)
		{
			Env->DeleteLocalRef(LocalHandler);
		}
		Env->DeleteLocalRef(LocalThread);
		return false;
	}
	SensorHandlerThread = Env->NewGlobalRef(LocalThread);
	SensorHandler = Env->NewGlobalRef(LocalHandler);
	Env->DeleteLocalRef(LocalHandler);
	Env->DeleteLocalRef(LocalThread);
	return SensorHandlerThread && SensorHandler;
#else
	return false;
#endif
}

void FOpenMobileSensorsAndroidBackend::StopSensorHandlerThread()
{
#if PLATFORM_ANDROID
	FScopeLock Lock(&HandlerMutex);
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		SensorHandlerThread = nullptr;
		SensorHandler = nullptr;
		return;
	}
	if (SensorHandlerThread)
	{
		jclass HandlerThreadClass = FAndroidApplication::FindJavaClass(
			"android/os/HandlerThread"
		);
		const jmethodID QuitSafelyMethod = HandlerThreadClass
			? Env->GetMethodID(
				HandlerThreadClass,
				"quitSafely",
				"()Z"
			)
			: nullptr;
		if (QuitSafelyMethod)
		{
			Env->CallBooleanMethod(
				static_cast<jobject>(SensorHandlerThread),
				QuitSafelyMethod
			);
		}
		Env->DeleteGlobalRef(static_cast<jobject>(SensorHandlerThread));
		SensorHandlerThread = nullptr;
	}
	if (SensorHandler)
	{
		Env->DeleteGlobalRef(static_cast<jobject>(SensorHandler));
		SensorHandler = nullptr;
	}
#endif
}

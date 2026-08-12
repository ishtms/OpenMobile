#include "OpenMobileSensorComponent.h"

#include "OpenMobileSensorActivityListeners.h"
#include "OpenMobileSensorPoseEnvironmentListeners.h"

UOpenMobileSensorComponent::UOpenMobileSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UOpenMobileSensorComponent::StartSensor()
{
	Activate(true);
}

void UOpenMobileSensorComponent::StopSensor()
{
	Deactivate();
}

UOpenMobileSensorListener*
UOpenMobileSensorComponent::GetActiveListener() const
{
	return ActiveListener.Get();
}

bool UOpenMobileSensorComponent::IsSensorActive() const
{
	return ActiveListener && ActiveListener->IsActive();
}

bool UOpenMobileSensorComponent::GetLatestVectorSample(
	FOpenMobileVectorSensorSample& OutSample) const
{
	OutSample = bHasVectorSample
		? LatestVectorSample
		: FOpenMobileVectorSensorSample{};
	return bHasVectorSample;
}

bool UOpenMobileSensorComponent::GetLatestAttitudeSample(
	FOpenMobileAttitudeSensorSample& OutSample) const
{
	OutSample = bHasAttitudeSample
		? LatestAttitudeSample
		: FOpenMobileAttitudeSensorSample{};
	return bHasAttitudeSample;
}

bool UOpenMobileSensorComponent::GetLatestScalarSample(
	FOpenMobileScalarSensorSample& OutSample) const
{
	OutSample = bHasScalarSample
		? LatestScalarSample
		: FOpenMobileScalarSensorSample{};
	return bHasScalarSample;
}

bool UOpenMobileSensorComponent::GetLatestHeadingSample(
	FOpenMobileHeadingSensorSample& OutSample) const
{
	OutSample = bHasHeadingSample
		? LatestHeadingSample
		: FOpenMobileHeadingSensorSample{};
	return bHasHeadingSample;
}

bool UOpenMobileSensorComponent::GetLatestStepsSample(
	FOpenMobileStepsSensorSample& OutSample) const
{
	OutSample = bHasStepsSample
		? LatestStepsSample
		: FOpenMobileStepsSensorSample{};
	return bHasStepsSample;
}

bool UOpenMobileSensorComponent::GetLatestActivitySample(
	FOpenMobileActivitySensorSample& OutSample) const
{
	OutSample = bHasActivitySample
		? LatestActivitySample
		: FOpenMobileActivitySensorSample{};
	return bHasActivitySample;
}

bool UOpenMobileSensorComponent::GetLatestOrientationSample(
	FOpenMobileOrientationSensorSample& OutSample) const
{
	OutSample = bHasOrientationSample
		? LatestOrientationSample
		: FOpenMobileOrientationSensorSample{};
	return bHasOrientationSample;
}

bool UOpenMobileSensorComponent::GetLatestProximitySample(
	FOpenMobileProximitySensorSample& OutSample) const
{
	OutSample = bHasProximitySample
		? LatestProximitySample
		: FOpenMobileProximitySensorSample{};
	return bHasProximitySample;
}

void UOpenMobileSensorComponent::Activate(bool bReset)
{
	if (ActiveListener && !bReset)
	{
		Super::Activate(false);
		return;
	}
	if (ActiveListener)
	{
		UOpenMobileSensorListener* Listener = ActiveListener.Get();
		Listener->Stop();
		if (ActiveListener == Listener)
		{
			ClearListener(Listener);
		}
	}

	Super::Activate(bReset);
	bHasVectorSample = false;
	bHasAttitudeSample = false;
	bHasScalarSample = false;
	bHasHeadingSample = false;
	bHasStepsSample = false;
	bHasActivitySample = false;
	bHasOrientationSample = false;
	bHasProximitySample = false;

	ActiveListener = CreateListener();
	if (!ActiveListener)
	{
		SetActiveFlag(false);
		BroadcastConfigurationFailure();
		return;
	}
	BindListener(*ActiveListener);
	ActiveListener->Activate();
}

void UOpenMobileSensorComponent::Deactivate()
{
	UOpenMobileSensorListener* Listener = ActiveListener.Get();
	if (Listener)
	{
		Listener->Stop();
		if (ActiveListener == Listener)
		{
			ClearListener(Listener);
		}
	}
	Super::Deactivate();
}

void UOpenMobileSensorComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (bStopWhenOwnerEndsPlay)
	{
		StopSensor();
	}
	Super::EndPlay(EndPlayReason);
}

UOpenMobileSensorListener* UOpenMobileSensorComponent::CreateListener() const
{
	UObject* ListenerOwner = GetOwner()
		? static_cast<UObject*>(GetOwner())
		: const_cast<UOpenMobileSensorComponent*>(this);
	switch (Sensor)
	{
	case EOpenMobileSensorType::Accelerometer:
		return UOpenMobileAccelerometerListener::ListenForAccelerometer(
			this, AdvancedOptions, RatePreset, CoordinateSpace,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::Gyroscope:
		return UOpenMobileGyroscopeListener::ListenForGyroscope(
			this, AdvancedOptions, RatePreset, CoordinateSpace,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::Magnetometer:
		return UOpenMobileMagnetometerListener::ListenForMagnetometer(
			this, AdvancedOptions, RatePreset, CoordinateSpace,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::Gravity:
		return UOpenMobileGravityListener::ListenForGravity(
			this, AdvancedOptions, RatePreset, CoordinateSpace,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::LinearAcceleration:
		return UOpenMobileLinearAccelerationListener::
			ListenForLinearAcceleration(
				this, AdvancedOptions, RatePreset, CoordinateSpace,
				bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::Shake:
		return UOpenMobileShakeListener::ListenForShake(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::Attitude:
		return UOpenMobileAttitudeListener::ListenForAttitude(
			this, AdvancedOptions, RatePreset, CoordinateSpace,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::MagneticHeading:
		return UOpenMobileMagneticHeadingListener::ListenForMagneticHeading(
			this, AdvancedOptions, RatePreset, CoordinateSpace,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::TrueHeading:
		return UOpenMobileTrueHeadingListener::ListenForTrueHeading(
			this, AdvancedOptions, RatePreset, CoordinateSpace,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::BarometricPressure:
		return UOpenMobilePressureListener::ListenForPressure(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::RelativeAltitude:
		return UOpenMobileRelativeAltitudeListener::ListenForRelativeAltitude(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::AbsoluteAltitude:
		return UOpenMobileAbsoluteAltitudeListener::ListenForAbsoluteAltitude(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::AmbientLight:
		return UOpenMobileAmbientLightListener::ListenForAmbientLight(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::Proximity:
		return UOpenMobileProximityListener::ListenForProximity(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::StepCounter:
		return UOpenMobileStepCountListener::ListenForStepCount(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::StepDetector:
		return UOpenMobileStepEventListener::ListenForStepEvents(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::Pedometer:
		return UOpenMobilePedometerListener::ListenForPedometer(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::MotionActivity:
		return UOpenMobileMotionActivityListener::ListenForMotionActivity(
			this, AdvancedOptions, RatePreset,
			bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::ActivityTransition:
		return UOpenMobileActivityTransitionListener::
			ListenForActivityTransitions(
				this, AdvancedOptions, RatePreset,
				bUseAdvancedOptions, ListenerOwner);
	case EOpenMobileSensorType::PhysicalOrientation:
		return UOpenMobilePhysicalOrientationListener::
			ListenForPhysicalOrientation(
				this, AdvancedOptions, RatePreset,
				bUseAdvancedOptions, ListenerOwner);
	default:
		return nullptr;
	}
}

void UOpenMobileSensorComponent::BindListener(
	UOpenMobileSensorListener& Listener)
{
	Listener.Started.AddDynamic(this, &UOpenMobileSensorComponent::HandleStarted);
	Listener.Paused.AddDynamic(this, &UOpenMobileSensorComponent::HandlePaused);
	Listener.Resumed.AddDynamic(this, &UOpenMobileSensorComponent::HandleResumed);
	Listener.PermissionRequired.AddDynamic(
		this, &UOpenMobileSensorComponent::HandlePermissionRequired);
	Listener.Unavailable.AddDynamic(
		this, &UOpenMobileSensorComponent::HandleUnavailable);
	Listener.Failed.AddDynamic(this, &UOpenMobileSensorComponent::HandleFailed);
	Listener.Stopped.AddDynamic(this, &UOpenMobileSensorComponent::HandleStopped);
	Listener.SamplesDropped.AddDynamic(
		this, &UOpenMobileSensorComponent::HandleSamplesDropped);
	Listener.SensorError.AddDynamic(
		this, &UOpenMobileSensorComponent::HandleSensorError);
	Listener.OnVectorSampleNative().AddUObject(
		this, &UOpenMobileSensorComponent::HandleVectorSample);
	Listener.OnAttitudeSampleNative().AddUObject(
		this, &UOpenMobileSensorComponent::HandleAttitudeSample);
	Listener.OnScalarSampleNative().AddUObject(
		this, &UOpenMobileSensorComponent::HandleScalarSample);
	Listener.OnHeadingSampleNative().AddUObject(
		this, &UOpenMobileSensorComponent::HandleHeadingSample);
	Listener.OnStepsSampleNative().AddUObject(
		this, &UOpenMobileSensorComponent::HandleStepsSample);
	Listener.OnActivitySampleNative().AddUObject(
		this, &UOpenMobileSensorComponent::HandleActivitySample);
	Listener.OnOrientationSampleNative().AddUObject(
		this, &UOpenMobileSensorComponent::HandleOrientationSample);
	Listener.OnProximitySampleNative().AddUObject(
		this, &UOpenMobileSensorComponent::HandleProximitySample);
}

void UOpenMobileSensorComponent::UnbindListener(
	UOpenMobileSensorListener& Listener)
{
	Listener.Started.RemoveDynamic(this, &UOpenMobileSensorComponent::HandleStarted);
	Listener.Paused.RemoveDynamic(this, &UOpenMobileSensorComponent::HandlePaused);
	Listener.Resumed.RemoveDynamic(this, &UOpenMobileSensorComponent::HandleResumed);
	Listener.PermissionRequired.RemoveDynamic(
		this, &UOpenMobileSensorComponent::HandlePermissionRequired);
	Listener.Unavailable.RemoveDynamic(
		this, &UOpenMobileSensorComponent::HandleUnavailable);
	Listener.Failed.RemoveDynamic(this, &UOpenMobileSensorComponent::HandleFailed);
	Listener.Stopped.RemoveDynamic(this, &UOpenMobileSensorComponent::HandleStopped);
	Listener.SamplesDropped.RemoveDynamic(
		this, &UOpenMobileSensorComponent::HandleSamplesDropped);
	Listener.SensorError.RemoveDynamic(
		this, &UOpenMobileSensorComponent::HandleSensorError);
	Listener.OnVectorSampleNative().RemoveAll(this);
	Listener.OnAttitudeSampleNative().RemoveAll(this);
	Listener.OnScalarSampleNative().RemoveAll(this);
	Listener.OnHeadingSampleNative().RemoveAll(this);
	Listener.OnStepsSampleNative().RemoveAll(this);
	Listener.OnActivitySampleNative().RemoveAll(this);
	Listener.OnOrientationSampleNative().RemoveAll(this);
	Listener.OnProximitySampleNative().RemoveAll(this);
}

void UOpenMobileSensorComponent::ClearListener(
	UOpenMobileSensorListener* Listener)
{
	if (!Listener || ActiveListener != Listener)
	{
		return;
	}
	UnbindListener(*Listener);
	ActiveListener = nullptr;
	SetActiveFlag(false);
}

void UOpenMobileSensorComponent::BroadcastConfigurationFailure()
{
	FOpenMobileSensorOperationResult Details;
	Details.Code = EOpenMobileSensorResultCode::InvalidArgument;
	const FText Message = FText::FromString(
		TEXT("The sensor component is configured with an unsupported sensor type."));
	const FText Correction = FText::FromString(
		TEXT("Choose one of the component's preferred sensor types or use the advanced raw subscription API."));
	SensorFailed.Broadcast(this, nullptr, Message, Correction, Details);
}

void UOpenMobileSensorComponent::BroadcastSample(
	EOpenMobileSensorSampleFamily SampleFamily,
	const FOpenMobileSensorSampleHeader& Header)
{
	Sample.Broadcast(
		this,
		ActiveListener.Get(),
		SampleFamily,
		UOpenMobileSensorListener::MakeSampleInfo(Header));
}

void UOpenMobileSensorComponent::HandleStarted(
	UOpenMobileSensorListener* Listener,
	FOpenMobileSensorStreamOptions AppliedOptions,
	double AppliedRateHz,
	EOpenMobileSensorAvailabilitySource Source,
	bool bRateAdjusted,
	EOpenMobileSensorLifecyclePolicy BackgroundBehavior)
{
	static_cast<void>(bRateAdjusted);
	static_cast<void>(BackgroundBehavior);
	SensorReady.Broadcast(
		this, Listener, AppliedOptions, AppliedRateHz, Source);
}

void UOpenMobileSensorComponent::HandlePaused(
	UOpenMobileSensorListener* Listener)
{
	SensorPaused.Broadcast(this, Listener);
}

void UOpenMobileSensorComponent::HandleResumed(
	UOpenMobileSensorListener* Listener)
{
	SensorResumed.Broadcast(this, Listener);
}

void UOpenMobileSensorComponent::HandlePermissionRequired(
	UOpenMobileSensorListener* Listener,
	FText Message,
	FText Correction,
	FOpenMobileSensorOperationResult Details)
{
	PermissionRequired.Broadcast(
		this, Listener, Message, Correction, Details);
	ClearListener(Listener);
}

void UOpenMobileSensorComponent::HandleUnavailable(
	UOpenMobileSensorListener* Listener,
	FText Message,
	FText Correction,
	FOpenMobileSensorOperationResult Details)
{
	SensorUnavailable.Broadcast(
		this, Listener, Message, Correction, Details);
	ClearListener(Listener);
}

void UOpenMobileSensorComponent::HandleFailed(
	UOpenMobileSensorListener* Listener,
	FText Message,
	FText Correction,
	FOpenMobileSensorOperationResult Details)
{
	SensorFailed.Broadcast(this, Listener, Message, Correction, Details);
	ClearListener(Listener);
}

void UOpenMobileSensorComponent::HandleStopped(
	UOpenMobileSensorListener* Listener)
{
	SensorStopped.Broadcast(this, Listener);
	ClearListener(Listener);
}

void UOpenMobileSensorComponent::HandleSamplesDropped(
	UOpenMobileSensorListener* Listener,
	FOpenMobileSensorDropInfo DropInfo)
{
	SamplesDropped.Broadcast(this, Listener, DropInfo);
}

void UOpenMobileSensorComponent::HandleSensorError(
	UOpenMobileSensorListener* Listener,
	FOpenMobileSensorRuntimeError Error)
{
	SensorError.Broadcast(this, Listener, Error);
}

void UOpenMobileSensorComponent::HandleVectorSample(
	const FOpenMobileVectorSensorSample& InSample)
{
	LatestVectorSample = InSample;
	bHasVectorSample = true;
	BroadcastSample(EOpenMobileSensorSampleFamily::Vector, InSample.Header);
}

void UOpenMobileSensorComponent::HandleAttitudeSample(
	const FOpenMobileAttitudeSensorSample& InSample)
{
	LatestAttitudeSample = InSample;
	bHasAttitudeSample = true;
	BroadcastSample(EOpenMobileSensorSampleFamily::Attitude, InSample.Header);
}

void UOpenMobileSensorComponent::HandleScalarSample(
	const FOpenMobileScalarSensorSample& InSample)
{
	LatestScalarSample = InSample;
	bHasScalarSample = true;
	BroadcastSample(EOpenMobileSensorSampleFamily::Scalar, InSample.Header);
}

void UOpenMobileSensorComponent::HandleHeadingSample(
	const FOpenMobileHeadingSensorSample& InSample)
{
	LatestHeadingSample = InSample;
	bHasHeadingSample = true;
	BroadcastSample(EOpenMobileSensorSampleFamily::Heading, InSample.Header);
}

void UOpenMobileSensorComponent::HandleStepsSample(
	const FOpenMobileStepsSensorSample& InSample)
{
	LatestStepsSample = InSample;
	bHasStepsSample = true;
	BroadcastSample(EOpenMobileSensorSampleFamily::Steps, InSample.Header);
}

void UOpenMobileSensorComponent::HandleActivitySample(
	const FOpenMobileActivitySensorSample& InSample)
{
	LatestActivitySample = InSample;
	bHasActivitySample = true;
	BroadcastSample(EOpenMobileSensorSampleFamily::Activity, InSample.Header);
}

void UOpenMobileSensorComponent::HandleOrientationSample(
	const FOpenMobileOrientationSensorSample& InSample)
{
	LatestOrientationSample = InSample;
	bHasOrientationSample = true;
	BroadcastSample(EOpenMobileSensorSampleFamily::Orientation, InSample.Header);
}

void UOpenMobileSensorComponent::HandleProximitySample(
	const FOpenMobileProximitySensorSample& InSample)
{
	LatestProximitySample = InSample;
	bHasProximitySample = true;
	BroadcastSample(EOpenMobileSensorSampleFamily::Proximity, InSample.Header);
}

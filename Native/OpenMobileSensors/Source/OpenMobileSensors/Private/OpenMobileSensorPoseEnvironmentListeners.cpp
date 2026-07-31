#include "OpenMobileSensorPoseEnvironmentListeners.h"

UOpenMobileAttitudeListener* UOpenMobileAttitudeListener::ListenForAttitude(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner)
{
	UOpenMobileAttitudeListener* Listener =
		NewObject<UOpenMobileAttitudeListener>();
	Listener->ConfigureListener(WorldContextObject, ListenerOwner,
		EOpenMobileSensorType::Attitude, AdvancedOptions, RatePreset,
		CoordinateSpace, bUseAdvancedOptions);
	return Listener;
}

bool UOpenMobileAttitudeListener::GetLatestAttitude(
	FQuat& OutRotation,
	bool& bOutHasEulerRotation,
	FRotator& OutEulerRotationDegrees,
	FOpenMobileSensorSampleInfo& OutSampleInfo) const
{
	OutRotation = FQuat::Identity;
	bOutHasEulerRotation = false;
	OutEulerRotationDegrees = FRotator::ZeroRotator;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutRotation = LatestSample.Quaternion;
	bOutHasEulerRotation = LatestSample.bHasEulerDegrees;
	OutEulerRotationDegrees = LatestSample.EulerDegrees;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

FOpenMobileSensorOperationResult
UOpenMobileAttitudeListener::SetAttitudeReferenceFrame(
	EOpenMobileAttitudeReferenceFrame ReferenceFrame)
{
	FOpenMobileSensorStreamOptions Options = GetAppliedOptions();
	Options.AttitudeReferenceFrame = ReferenceFrame;
	return UpdateListenerOptions(Options);
}

void UOpenMobileAttitudeListener::RecenterAttitude(
	EOpenMobileSensorRecenterMode Mode,
	EOpenMobileSensorControlOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details)
{
	ResolveControlOutcome(RecenterListenerAttitude(Mode),
		Outcome, Message, Correction, Details);
}

void UOpenMobileAttitudeListener::ClearAttitudeRecenter(
	EOpenMobileSensorControlOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details)
{
	ResolveControlOutcome(
		RecenterListenerAttitude(EOpenMobileSensorRecenterMode::Clear),
		Outcome, Message, Correction, Details);
}

void UOpenMobileAttitudeListener::RequestAttitudeCalibration(
	EOpenMobileSensorControlOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details)
{
	ResolveControlOutcome(RequestListenerCalibration(),
		Outcome, Message, Correction, Details);
}

void UOpenMobileAttitudeListener::HandleAttitudeSample(
	const FOpenMobileAttitudeSensorSample& InSample)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Quaternion,
		LatestSample.bHasEulerDegrees, LatestSample.EulerDegrees,
		MakeSampleInfo(LatestSample.Header));
}

#define OPENMOBILE_IMPLEMENT_HEADING_LISTENER(ListenerClass, FactoryName, GetterName, SensorType) \
	ListenerClass* ListenerClass::FactoryName( \
		const UObject* WorldContextObject, \
		const FOpenMobileSensorStreamOptions& AdvancedOptions, \
		EOpenMobileSensorRatePreset RatePreset, \
		EOpenMobileSensorCoordinateSpace CoordinateSpace, \
		bool bUseAdvancedOptions, UObject* ListenerOwner) \
	{ \
		ListenerClass* Listener = NewObject<ListenerClass>(); \
		Listener->ConfigureListener(WorldContextObject, ListenerOwner, \
			SensorType, AdvancedOptions, RatePreset, CoordinateSpace, \
			bUseAdvancedOptions); \
		return Listener; \
	} \
	bool ListenerClass::GetterName(double& OutValue, \
		FOpenMobileSensorSampleInfo& OutSampleInfo) const \
	{ \
		OutValue = 0.0; \
		OutSampleInfo = {}; \
		if (!bHasSample) \
		{ \
			return false; \
		} \
		OutValue = LatestSample.HeadingDegrees; \
		OutSampleInfo = MakeSampleInfo(LatestSample.Header); \
		return true; \
	} \
	void ListenerClass::HandleHeadingSample( \
		const FOpenMobileHeadingSensorSample& InSample) \
	{ \
		LatestSample = InSample; \
		bHasSample = true; \
		Sample.Broadcast(this, LatestSample.HeadingDegrees, \
			MakeSampleInfo(LatestSample.Header)); \
	}

OPENMOBILE_IMPLEMENT_HEADING_LISTENER(
	UOpenMobileMagneticHeadingListener,
	ListenForMagneticHeading,
	GetLatestHeading,
	EOpenMobileSensorType::MagneticHeading
)
OPENMOBILE_IMPLEMENT_HEADING_LISTENER(
	UOpenMobileTrueHeadingListener,
	ListenForTrueHeading,
	GetLatestHeading,
	EOpenMobileSensorType::TrueHeading
)

#undef OPENMOBILE_IMPLEMENT_HEADING_LISTENER

void UOpenMobileMagneticHeadingListener::RequestHeadingCalibration(
	EOpenMobileSensorControlOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details)
{
	ResolveControlOutcome(RequestListenerCalibration(),
		Outcome, Message, Correction, Details);
}

#define OPENMOBILE_IMPLEMENT_SCALAR_LISTENER(ListenerClass, FactoryName, GetterName, SensorType) \
	ListenerClass* ListenerClass::FactoryName( \
		const UObject* WorldContextObject, \
		const FOpenMobileSensorStreamOptions& AdvancedOptions, \
		EOpenMobileSensorRatePreset RatePreset, bool bUseAdvancedOptions, \
		UObject* ListenerOwner) \
	{ \
		ListenerClass* Listener = NewObject<ListenerClass>(); \
		Listener->ConfigureListener(WorldContextObject, ListenerOwner, \
			SensorType, AdvancedOptions, RatePreset, \
			EOpenMobileSensorCoordinateSpace::DeviceFixed, bUseAdvancedOptions); \
		return Listener; \
	} \
	bool ListenerClass::GetterName(double& OutValue, \
		FOpenMobileSensorSampleInfo& OutSampleInfo) const \
	{ \
		OutValue = 0.0; \
		OutSampleInfo = {}; \
		if (!bHasSample) \
		{ \
			return false; \
		} \
		OutValue = LatestSample.Value; \
		OutSampleInfo = MakeSampleInfo(LatestSample.Header); \
		return true; \
	} \
	void ListenerClass::HandleScalarSample( \
		const FOpenMobileScalarSensorSample& InSample) \
	{ \
		LatestSample = InSample; \
		bHasSample = true; \
		Sample.Broadcast(this, LatestSample.Value, \
			MakeSampleInfo(LatestSample.Header)); \
	}

OPENMOBILE_IMPLEMENT_SCALAR_LISTENER(
	UOpenMobilePressureListener,
	ListenForPressure,
	GetLatestPressure,
	EOpenMobileSensorType::BarometricPressure
)
OPENMOBILE_IMPLEMENT_SCALAR_LISTENER(
	UOpenMobileRelativeAltitudeListener,
	ListenForRelativeAltitude,
	GetLatestAltitude,
	EOpenMobileSensorType::RelativeAltitude
)
OPENMOBILE_IMPLEMENT_SCALAR_LISTENER(
	UOpenMobileAbsoluteAltitudeListener,
	ListenForAbsoluteAltitude,
	GetLatestAltitude,
	EOpenMobileSensorType::AbsoluteAltitude
)
OPENMOBILE_IMPLEMENT_SCALAR_LISTENER(
	UOpenMobileAmbientLightListener,
	ListenForAmbientLight,
	GetLatestIlluminance,
	EOpenMobileSensorType::AmbientLight
)

#undef OPENMOBILE_IMPLEMENT_SCALAR_LISTENER

UOpenMobileProximityListener* UOpenMobileProximityListener::ListenForProximity(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner)
{
	UOpenMobileProximityListener* Listener =
		NewObject<UOpenMobileProximityListener>();
	Listener->ConfigureListener(WorldContextObject, ListenerOwner,
		EOpenMobileSensorType::Proximity, AdvancedOptions, RatePreset,
		EOpenMobileSensorCoordinateSpace::DeviceFixed, bUseAdvancedOptions);
	return Listener;
}

bool UOpenMobileProximityListener::GetLatestProximity(
	bool& bOutIsNear,
	bool& bOutHasDistance,
	double& OutDistanceMetres,
	FOpenMobileSensorSampleInfo& OutSampleInfo) const
{
	bOutIsNear = false;
	bOutHasDistance = false;
	OutDistanceMetres = 0.0;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	bOutIsNear = LatestSample.bNear;
	bOutHasDistance = LatestSample.bHasDistanceMeters;
	OutDistanceMetres = LatestSample.DistanceMeters;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileProximityListener::HandleProximitySample(
	const FOpenMobileProximitySensorSample& InSample)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.bNear,
		LatestSample.bHasDistanceMeters, LatestSample.DistanceMeters,
		MakeSampleInfo(LatestSample.Header));
}

UOpenMobilePhysicalOrientationListener*
UOpenMobilePhysicalOrientationListener::ListenForPhysicalOrientation(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner)
{
	UOpenMobilePhysicalOrientationListener* Listener =
		NewObject<UOpenMobilePhysicalOrientationListener>();
	Listener->ConfigureListener(WorldContextObject, ListenerOwner,
		EOpenMobileSensorType::PhysicalOrientation, AdvancedOptions, RatePreset,
		EOpenMobileSensorCoordinateSpace::DeviceFixed, bUseAdvancedOptions);
	return Listener;
}

bool UOpenMobilePhysicalOrientationListener::GetLatestOrientation(
	EOpenMobilePhysicalOrientation& OutOrientation,
	double& OutConfidence,
	FOpenMobileSensorSampleInfo& OutSampleInfo) const
{
	OutOrientation = EOpenMobilePhysicalOrientation::Unknown;
	OutConfidence = 0.0;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutOrientation = LatestSample.Orientation;
	OutConfidence = LatestSample.Confidence;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobilePhysicalOrientationListener::HandleOrientationSample(
	const FOpenMobileOrientationSensorSample& InSample)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Orientation, LatestSample.Confidence,
		MakeSampleInfo(LatestSample.Header));
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorScreenRotation.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsScreenRotationTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue,
		EOpenMobileSensorType SensorType =
			EOpenMobileSensorType::Accelerometer
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = SensorType;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 100.0;
		Request.Options.MaximumCallbackFrequencyHz = 100.0;
		Request.Options.CoordinateSpace = CoordinateSpace;
		Request.Options.DeliveryMode = DeliveryMode;
		Request.Options.BufferCapacitySamples = 16;
		return Request;
	}

	FOpenMobileVectorSensorSample MakeVector(
		double TimestampSeconds,
		const FVector& Value
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = MakeRequest(
			EOpenMobileSensorCoordinateSpace::DeviceFixed).Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Value = Value;
		return Sample;
	}

	FOpenMobileSensorSubscriptionResult StartActive(
		const FGuid& Owner,
		const FOpenMobileSensorSubscriptionRequest& Request
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner, Request);
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		return Result;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsScreenRotationService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsScreenRotationService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsScreenFourRotationsTest,
	"OpenMobile.Sensors.ScreenRotation.FourRotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsScreenFourRotationsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FVector DeviceTop = FVector::ForwardVector;
	TestEqual(TEXT("Zero keeps device top"),
		FOpenMobileSensorsScreenRotationService::RotateVector(
			DeviceTop, EOpenMobileSensorScreenRotation::Rotation0),
		FVector::ForwardVector);
	TestEqual(TEXT("Ninety maps device top to screen left"),
		FOpenMobileSensorsScreenRotationService::RotateVector(
			DeviceTop, EOpenMobileSensorScreenRotation::Rotation90),
		-FVector::RightVector);
	TestEqual(TEXT("One eighty maps device top to screen bottom"),
		FOpenMobileSensorsScreenRotationService::RotateVector(
			DeviceTop, EOpenMobileSensorScreenRotation::Rotation180),
		-FVector::ForwardVector);
	TestEqual(TEXT("Two seventy maps device top to screen right"),
		FOpenMobileSensorsScreenRotationService::RotateVector(
			DeviceTop, EOpenMobileSensorScreenRotation::Rotation270),
		FVector::RightVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsScreenSharedPhysicalStreamTest,
	"OpenMobile.Sensors.ScreenRotation.SharedPhysicalStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsScreenSharedPhysicalStreamTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsScreenRotationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ScreenSharing"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner,
		EOpenMobileSensorScreenRotation::Rotation90,
		1.0,
		false
	);
	const FOpenMobileSensorSubscriptionResult Device = StartActive(
		Owner,
		MakeRequest(EOpenMobileSensorCoordinateSpace::DeviceFixed)
	);
	const FOpenMobileSensorSubscriptionResult Screen = StartActive(
		Owner,
		MakeRequest(EOpenMobileSensorCoordinateSpace::CurrentScreen)
	);
	TestEqual(TEXT("Coordinate choices share one physical stream"),
		Backend.GetStartSensorStreamCount(), 1);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeVector(2.0, FVector::ForwardVector));
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample DeviceSample;
	FOpenMobileVectorSensorSample ScreenSample;
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner, Device.Handle, 0, 2.0, Read, DeviceSample);
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner, Screen.Handle, 0, 2.0, Read, ScreenSample);
	TestEqual(TEXT("Device-fixed output remains unchanged"),
		DeviceSample.Value, FVector::ForwardVector);
	TestEqual(TEXT("Current-screen output follows rotation"),
		ScreenSample.Value, -FVector::RightVector);
	TestEqual(TEXT("The screen output reports its coordinate space"),
		ScreenSample.Header.CoordinateSpace,
		EOpenMobileSensorCoordinateSpace::CurrentScreen);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsScreenBufferedBoundaryTest,
	"OpenMobile.Sensors.ScreenRotation.BufferedBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsScreenBufferedBoundaryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsScreenRotationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ScreenBoundary"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner, EOpenMobileSensorScreenRotation::Rotation0, 1.0, false);
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner, EOpenMobileSensorScreenRotation::Rotation90, 10.0, false);
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		MakeRequest(
			EOpenMobileSensorCoordinateSpace::CurrentScreen,
			EOpenMobileSensorDeliveryMode::Buffered
		)
	);
	FOpenMobileVectorSensorBatch Published;
	Published.Samples = {
		MakeVector(9.0, FVector::ForwardVector),
		MakeVector(11.0, FVector::ForwardVector)
	};
	FOpenMobileSensorsSampleService::PublishVectorBatch(Published);
	FOpenMobileSensorBufferReadResult Read;
	FOpenMobileVectorSensorBatch Buffered;
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		Owner, Subscription.Handle, 16, Read, Buffered);
	TestEqual(TEXT("Both sides of the boundary remain buffered"),
		Buffered.Samples.Num(), 2);
	TestEqual(TEXT("The pre-rotation sample uses the old transform"),
		Buffered.Samples[0].Value, FVector::ForwardVector);
	TestEqual(TEXT("The post-rotation sample uses the new transform"),
		Buffered.Samples[1].Value, -FVector::RightVector);
	TestEqual(TEXT("The old transform is identified"),
		Buffered.Samples[0].Header.ScreenRotation,
		EOpenMobileSensorScreenRotation::Rotation0);
	TestEqual(TEXT("The new transform is identified"),
		Buffered.Samples[1].Header.ScreenRotation,
		EOpenMobileSensorScreenRotation::Rotation90);
	TestTrue(TEXT("Transform sequences explain the batch split"),
		Buffered.Samples[0].Header.ScreenRotationSequence <
			Buffered.Samples[1].Header.ScreenRotationSequence);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsScreenNaturalOrientationTest,
	"OpenMobile.Sensors.ScreenRotation.NaturalOrientation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsScreenNaturalOrientationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsScreenRotationTestsPrivate;
	ResetServices();
	const FGuid PortraitOwner = FGuid::NewGuid();
	const FGuid LandscapeOwner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		PortraitOwner, EOpenMobileSensorScreenRotation::Rotation90, 1.0, false);
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		LandscapeOwner, EOpenMobileSensorScreenRotation::Rotation90, 1.0, true);
	FOpenMobileSensorScreenRotationSnapshot Portrait;
	FOpenMobileSensorScreenRotationSnapshot Landscape;
	FOpenMobileSensorsScreenRotationService::ResolveRotation(
		PortraitOwner, 2.0, Portrait);
	FOpenMobileSensorsScreenRotationService::ResolveRotation(
		LandscapeOwner, 2.0, Landscape);
	TestFalse(TEXT("Portrait-natural state remains explicit"),
		Portrait.bNaturalOrientationLandscape);
	TestTrue(TEXT("Landscape-natural state remains explicit"),
		Landscape.bNaturalOrientationLandscape);
	TestEqual(TEXT("Rotation is relative to each natural orientation"),
		Portrait.Rotation, Landscape.Rotation);
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsScreenAttitudeAndHeadingTest,
	"OpenMobile.Sensors.ScreenRotation.AttitudeAndHeading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsScreenAttitudeAndHeadingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsScreenRotationTestsPrivate;
	ResetServices();
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner, EOpenMobileSensorScreenRotation::Rotation90, 1.0, false);
	FOpenMobileAttitudeSensorSample Attitude;
	Attitude.Header.TimestampSeconds = 2.0;
	Attitude.Header.bValid = true;
	Attitude.Quaternion = FQuat(
		FVector::ForwardVector,
		UE_DOUBLE_PI * 0.5
	);
	Attitude.bHasEulerDegrees = true;
	Attitude.bHasRotationMatrix = true;
	FOpenMobileSensorsScreenRotationService::ApplyToSample(
		Owner,
		EOpenMobileSensorCoordinateSpace::CurrentScreen,
		Attitude
	);
	const FQuat Expected(-FVector::RightVector, UE_DOUBLE_PI * 0.5);
	TestTrue(TEXT("Attitude uses the same screen basis"),
		FMath::Abs(Attitude.Quaternion | Expected) > 1.0 - 1.e-9);
	TestEqual(TEXT("The attitude matrix follows the quaternion"),
		Attitude.RotationMatrix.XAxis,
		Attitude.Quaternion.RotateVector(FVector::ForwardVector));
	FOpenMobileHeadingSensorSample Heading;
	Heading.Header.TimestampSeconds = 2.0;
	Heading.Header.bValid = true;
	Heading.HeadingDegrees = 10.0;
	FOpenMobileSensorsScreenRotationService::ApplyToSample(
		Owner,
		EOpenMobileSensorCoordinateSpace::CurrentScreen,
		Heading
	);
	TestEqual(TEXT("Heading follows the current screen top"),
		Heading.HeadingDegrees, 100.0);
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsScreenSplitScreenAndOrientationLockTest,
	"OpenMobile.Sensors.ScreenRotation.SplitScreenAndOrientationLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsScreenSplitScreenAndOrientationLockTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsScreenRotationTestsPrivate;
	ResetServices();
	const FGuid LockedOwner = FGuid::NewGuid();
	const FGuid RotatingOwner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		LockedOwner, EOpenMobileSensorScreenRotation::Rotation0, 1.0, false);
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		RotatingOwner, EOpenMobileSensorScreenRotation::Rotation270, 2.0, false);
	FOpenMobileSensorScreenRotationSnapshot Locked;
	FOpenMobileSensorScreenRotationSnapshot Rotating;
	FOpenMobileSensorsScreenRotationService::ResolveRotation(
		LockedOwner, 3.0, Locked);
	FOpenMobileSensorsScreenRotationService::ResolveRotation(
		RotatingOwner, 3.0, Rotating);
	TestEqual(TEXT("An orientation-locked owner stays fixed"),
		Locked.Rotation, EOpenMobileSensorScreenRotation::Rotation0);
	TestEqual(TEXT("A second window keeps independent rotation"),
		Rotating.Rotation, EOpenMobileSensorScreenRotation::Rotation270);
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsScreenPhysicalOrientationSeparationTest,
	"OpenMobile.Sensors.ScreenRotation.PhysicalOrientationSeparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsScreenPhysicalOrientationSeparationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsScreenRotationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PhysicalOrientation"));
	FOpenMobileSensorCapability Capability;
	Capability.Sensor = MakeRequest(
		EOpenMobileSensorCoordinateSpace::DeviceFixed,
		EOpenMobileSensorDeliveryMode::LatestValue,
		EOpenMobileSensorType::PhysicalOrientation
	).Sensor;
	Capability.Availability.State = EOpenMobileCapabilityState::Available;
	Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
	Backend.SetSensorCapabilities({Capability});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner, EOpenMobileSensorScreenRotation::Rotation180, 1.0, false);
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		MakeRequest(
			EOpenMobileSensorCoordinateSpace::CurrentScreen,
			EOpenMobileSensorDeliveryMode::LatestValue,
			EOpenMobileSensorType::PhysicalOrientation
		)
	);
	FOpenMobileOrientationSensorSample Published;
	Published.Header.Sensor = MakeRequest(
		EOpenMobileSensorCoordinateSpace::CurrentScreen,
		EOpenMobileSensorDeliveryMode::LatestValue,
		EOpenMobileSensorType::PhysicalOrientation).Sensor;
	Published.Header.TimestampSeconds = 2.0;
	Published.Header.bValid = true;
	Published.Orientation = EOpenMobilePhysicalOrientation::FaceUp;
	FOpenMobileSensorsSampleService::PublishOrientation(Published);
	FOpenMobileSensorReadResult Read;
	FOpenMobileOrientationSensorSample Result;
	FOpenMobileSensorsSampleService::ReadLatestOrientation(
		Owner, Subscription.Handle, 0, 2.0, Read, Result);
	TestEqual(TEXT("Physical orientation is not screen-compensated"),
		Result.Header.CoordinateSpace,
		EOpenMobileSensorCoordinateSpace::DeviceFixed);
	TestEqual(TEXT("Physical orientation remains the native state"),
		Result.Orientation, EOpenMobilePhysicalOrientation::FaceUp);
	FinishBackend(Backend);
	return true;
}

#endif

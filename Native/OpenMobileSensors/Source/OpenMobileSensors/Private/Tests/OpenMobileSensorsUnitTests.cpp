#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorUnits.h"

#include <limits>

namespace OpenMobileSensorsUnitTestsPrivate
{
	FOpenMobileSensorSampleHeader MakeHeader(EOpenMobileSensorType SensorType)
	{
		FOpenMobileSensorSampleHeader Header;
		Header.Sensor.Type = SensorType;
		Header.Sensor.InstanceId = TEXT("Default");
		Header.TimestampSeconds = 1.0;
		Header.bValid = true;
		return Header;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAndroidVectorUnitTest,
	"OpenMobile.Sensors.Units.Android.Vector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAndroidVectorUnitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsUnitTestsPrivate;
	FOpenMobileVectorSensorSample Acceleration;
	Acceleration.Header = MakeHeader(EOpenMobileSensorType::Accelerometer);
	Acceleration.Value = FVector(1.0, -2.0, 9.80665);
	TestTrue(TEXT("Android acceleration normalizes"),
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(
			EOpenMobileSensorNativePlatform::Android, Acceleration));
	TestEqual(TEXT("Android acceleration is already metres per second squared"),
		Acceleration.Value, FVector(1.0, -2.0, 9.80665));
	TestTrue(TEXT("The public sample is marked normalized"),
		Acceleration.Header.bUnitsNormalized);
	const FVector Once = Acceleration.Value;
	FOpenMobileSensorUnitConverter::NormalizeVectorSample(
		EOpenMobileSensorNativePlatform::Android, Acceleration);
	TestEqual(TEXT("A normalized sample is not converted twice"),
		Acceleration.Value, Once);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsIOSVectorUnitTest,
	"OpenMobile.Sensors.Units.IOS.Vector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsIOSVectorUnitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsUnitTestsPrivate;
	FOpenMobileVectorSensorSample Acceleration;
	Acceleration.Header = MakeHeader(EOpenMobileSensorType::Accelerometer);
	Acceleration.Value = FVector(0.0, 0.0, 1.0);
	FOpenMobileSensorUnitConverter::NormalizeVectorSample(
		EOpenMobileSensorNativePlatform::IOS, Acceleration);
	TestTrue(TEXT("Core Motion g converts to metres per second squared"),
		FMath::IsNearlyEqual(Acceleration.Value.Z, 9.80665, 1.e-9));
	FOpenMobileVectorSensorSample Gyroscope;
	Gyroscope.Header = MakeHeader(EOpenMobileSensorType::Gyroscope);
	Gyroscope.Value = FVector(1.0, 2.0, 3.0);
	FOpenMobileSensorUnitConverter::NormalizeVectorSample(
		EOpenMobileSensorNativePlatform::IOS, Gyroscope);
	TestEqual(TEXT("Core Motion rotation rate remains radians per second"),
		Gyroscope.Value, FVector(1.0, 2.0, 3.0));
	FOpenMobileVectorSensorSample Magnetometer;
	Magnetometer.Header = MakeHeader(EOpenMobileSensorType::Magnetometer);
	Magnetometer.Value = FVector(25.0, -10.0, 40.0);
	FOpenMobileSensorUnitConverter::NormalizeVectorSample(
		EOpenMobileSensorNativePlatform::IOS, Magnetometer);
	TestEqual(TEXT("Core Motion magnetic field remains microteslas"),
		Magnetometer.Value, FVector(25.0, -10.0, 40.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsScalarUnitTest,
	"OpenMobile.Sensors.Units.Android.IOS.Scalar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsScalarUnitTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsUnitTestsPrivate;
	FOpenMobileScalarSensorSample AndroidPressure;
	AndroidPressure.Header = MakeHeader(
		EOpenMobileSensorType::BarometricPressure);
	AndroidPressure.Value = 1013.25;
	FOpenMobileSensorUnitConverter::NormalizeScalarSample(
		EOpenMobileSensorNativePlatform::Android, AndroidPressure);
	TestEqual(TEXT("Android pressure remains hectopascals"),
		AndroidPressure.Value, 1013.25);
	FOpenMobileScalarSensorSample IOSPressure;
	IOSPressure.Header = MakeHeader(EOpenMobileSensorType::BarometricPressure);
	IOSPressure.Value = 101.325;
	FOpenMobileSensorUnitConverter::NormalizeScalarSample(
		EOpenMobileSensorNativePlatform::IOS, IOSPressure);
	TestEqual(TEXT("Core Motion kilopascals convert to hectopascals"),
		IOSPressure.Value, 1013.25);
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		FOpenMobileScalarSensorSample Altitude;
		Altitude.Header = MakeHeader(EOpenMobileSensorType::RelativeAltitude);
		Altitude.Value = 12.5;
		FOpenMobileSensorUnitConverter::NormalizeScalarSample(
			Platform, Altitude);
		TestEqual(TEXT("Altitude remains metres"), Altitude.Value, 12.5);
		FOpenMobileScalarSensorSample Light;
		Light.Header = MakeHeader(EOpenMobileSensorType::AmbientLight);
		Light.Value = 450.0;
		FOpenMobileSensorUnitConverter::NormalizeScalarSample(Platform, Light);
		TestEqual(TEXT("Ambient light remains lux"), Light.Value, 450.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeHeadingUnitTest,
	"OpenMobile.Sensors.Units.Android.IOS.Attitude.Heading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeHeadingUnitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsUnitTestsPrivate;
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		FOpenMobileAttitudeSensorSample Attitude;
		Attitude.Header = MakeHeader(EOpenMobileSensorType::Attitude);
		Attitude.Quaternion = FQuat(0.0, 0.0, 0.0, 2.0);
		Attitude.bHasEulerDegrees = true;
		Attitude.EulerDegrees = FRotator(190.0, 540.0, -190.0);
		TestTrue(TEXT("Attitude output normalizes"),
			FOpenMobileSensorUnitConverter::NormalizeAttitudeSample(
				Platform, Attitude));
		TestTrue(TEXT("The quaternion has unit length"),
			FMath::IsNearlyEqual(Attitude.Quaternion.SizeSquared(), 1.0));
		TestTrue(TEXT("Euler pitch is in the documented range"),
			Attitude.EulerDegrees.Pitch >= -180.0
			&& Attitude.EulerDegrees.Pitch < 180.0);
		FOpenMobileHeadingSensorSample Heading;
		Heading.Header = MakeHeader(EOpenMobileSensorType::MagneticHeading);
		Heading.HeadingDegrees = -10.0;
		Heading.Reference = EOpenMobileHeadingReference::MagneticNorth;
		Heading.bTiltCompensated = true;
		FOpenMobileSensorUnitConverter::NormalizeHeadingSample(
			Platform, Heading);
		TestEqual(TEXT("Heading wraps to zero through 360 degrees"),
			Heading.HeadingDegrees, 350.0);
		TestEqual(TEXT("Heading keeps its north reference"),
			Heading.Reference,
			EOpenMobileHeadingReference::MagneticNorth);
		TestTrue(TEXT("Heading keeps its attitude compensation state"),
			Heading.bTiltCompensated);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStepsActivityOrientationUnitTest,
	"OpenMobile.Sensors.Units.Android.IOS.Steps.Activity.Orientation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStepsActivityOrientationUnitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsUnitTestsPrivate;
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		FOpenMobileStepsSensorSample Steps;
		Steps.Header = MakeHeader(EOpenMobileSensorType::Pedometer);
		Steps.Count = 42;
		Steps.Origin = EOpenMobileStepCountOrigin::Session;
		TestTrue(TEXT("Nonnegative steps normalize"),
			FOpenMobileSensorUnitConverter::NormalizeStepsSample(
				Platform, Steps));
		TestEqual(TEXT("Step origin remains explicit"),
			Steps.Origin, EOpenMobileStepCountOrigin::Session);
		FOpenMobileActivitySensorSample Activity;
		Activity.Header = MakeHeader(EOpenMobileSensorType::MotionActivity);
		Activity.Confidence = EOpenMobileActivityConfidence::Medium;
		TestTrue(TEXT("Named activity confidence normalizes"),
			FOpenMobileSensorUnitConverter::NormalizeActivitySample(
				Platform, Activity));
		TestEqual(TEXT("Activity confidence stays named"),
			Activity.Confidence, EOpenMobileActivityConfidence::Medium);
		FOpenMobileOrientationSensorSample Orientation;
		Orientation.Header = MakeHeader(
			EOpenMobileSensorType::PhysicalOrientation);
		Orientation.Confidence = 1.5;
		FOpenMobileSensorUnitConverter::NormalizeOrientationSample(
			Platform, Orientation);
		TestEqual(TEXT("Orientation confidence clamps to one"),
			Orientation.Confidence, 1.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsProximityUnitTest,
	"OpenMobile.Sensors.Units.Android.IOS.Proximity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsProximityUnitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsUnitTestsPrivate;
	FOpenMobileProximitySensorSample Android;
	Android.Header = MakeHeader(EOpenMobileSensorType::Proximity);
	Android.bHasDistanceMeters = true;
	Android.DistanceMeters = 5.0;
	Android.bHasMaximumRangeMeters = true;
	Android.MaximumRangeMeters = 10.0;
	TestTrue(TEXT("Android proximity values normalize"),
		FOpenMobileSensorUnitConverter::NormalizeProximitySample(
			EOpenMobileSensorNativePlatform::Android, Android));
	TestEqual(TEXT("Android proximity centimetres convert to metres"),
		Android.DistanceMeters, 0.05);
	TestEqual(TEXT("Android maximum range converts to metres"),
		Android.MaximumRangeMeters, 0.1);
	TestTrue(TEXT("Android distance below maximum range is near"),
		Android.bNear);
	FOpenMobileProximitySensorSample OutOfRange;
	OutOfRange.Header = MakeHeader(EOpenMobileSensorType::Proximity);
	OutOfRange.bHasDistanceMeters = true;
	OutOfRange.DistanceMeters = 10.01;
	OutOfRange.bHasMaximumRangeMeters = true;
	OutOfRange.MaximumRangeMeters = 10.0;
	TestFalse(TEXT("Distance beyond the native range is rejected"),
		FOpenMobileSensorUnitConverter::NormalizeProximitySample(
			EOpenMobileSensorNativePlatform::Android, OutOfRange));
	TestFalse(TEXT("Rejected proximity is marked invalid"),
		OutOfRange.Header.bValid);
	FOpenMobileProximitySensorSample IOS;
	IOS.Header = MakeHeader(EOpenMobileSensorType::Proximity);
	IOS.bNear = true;
	IOS.DistanceMeters = std::numeric_limits<double>::quiet_NaN();
	IOS.MaximumRangeMeters = 123.0;
	TestTrue(TEXT("iOS state-only proximity normalizes"),
		FOpenMobileSensorUnitConverter::NormalizeProximitySample(
			EOpenMobileSensorNativePlatform::IOS, IOS));
	TestTrue(TEXT("iOS proximity remains a near-state flag"), IOS.bNear);
	TestFalse(TEXT("iOS does not invent a distance"), IOS.bHasDistanceMeters);
	TestEqual(TEXT("Missing iOS distance is canonical"),
		IOS.DistanceMeters, 0.0);
	TestFalse(TEXT("iOS does not invent a maximum range"),
		IOS.bHasMaximumRangeMeters);
	TestEqual(TEXT("Missing iOS maximum range is canonical"),
		IOS.MaximumRangeMeters, 0.0);
	return true;
}

#if !UE_BUILD_SHIPPING
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeUnitDiagnosticsTest,
	"OpenMobile.Sensors.Units.DevelopmentDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeUnitDiagnosticsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsUnitTestsPrivate;
	FOpenMobileVectorSensorSample Sample;
	Sample.Header = MakeHeader(EOpenMobileSensorType::Accelerometer);
	Sample.Value = FVector(
		1.e20,
		std::numeric_limits<double>::quiet_NaN(),
		-1.e20
	);
	FOpenMobileSensorNativeUnitDiagnostics Diagnostics;
	FOpenMobileSensorUnitConverter::NormalizeVectorSample(
		EOpenMobileSensorNativePlatform::Android,
		Sample,
		&Diagnostics
	);
	TestEqual(TEXT("Only supplied native components are captured"),
		Diagnostics.SanitizedValueCount, 3);
	TestEqual(TEXT("Large positive diagnostics are bounded"),
		Diagnostics.SanitizedValues[0], 1.e12);
	TestEqual(TEXT("Non-finite diagnostics are sanitized"),
		Diagnostics.SanitizedValues[1], 0.0);
	TestEqual(TEXT("Large negative diagnostics are bounded"),
		Diagnostics.SanitizedValues[2], -1.e12);
	return true;
}
#endif

#endif

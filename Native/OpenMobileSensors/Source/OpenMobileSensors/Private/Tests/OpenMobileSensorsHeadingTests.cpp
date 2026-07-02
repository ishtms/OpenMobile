#include "Misc/AutomationTest.h"

#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorHeading.h"
#include "OpenMobileSensorScreenRotationService.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAndroidMagneticHeadingFixturesTest,
	"OpenMobile.Sensors.Heading.Magnetic.AndroidRotationVectorFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAndroidMagneticHeadingFixturesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	struct FFixture
	{
		const TCHAR* Name;
		FQuat RotationVector;
		double ExpectedHeadingDegrees;
	};
	const FFixture Fixtures[] = {
		{TEXT("North"), FQuat(0.0, 0.0, 0.0, 1.0), 0.0},
		{TEXT("East"), FQuat(0.0, 0.0, -0.70710678, 0.70710678), 90.0},
		{TEXT("South"), FQuat(0.0, 0.0, -1.0, 0.0), 180.0},
		{TEXT("West"), FQuat(0.0, 0.0, -0.70710678, -0.70710678), 270.0},
		{TEXT("Wraparound"), FQuat(0.0, 0.0, -0.00872654, -0.99996192), 359.0},
		{TEXT("TiltedNorth"), FQuat(0.5, 0.0, 0.0, 0.8660254), 0.0},
		{TEXT("TiltedEast"), FQuat(0.35355339, -0.35355339, -0.61237244, 0.61237244), 90.0},
		{TEXT("TiltedSouth"), FQuat(0.0, -0.5, -0.8660254, 0.0), 180.0},
		{TEXT("TiltedWest"), FQuat(-0.35355339, -0.35355339, -0.61237244, -0.61237244), 270.0}
	};
	for (const FFixture& Fixture : Fixtures)
	{
		double HeadingDegrees = -1.0;
		TestTrue(FString::Printf(TEXT("%s fixture is valid"), Fixture.Name),
			FOpenMobileSensorHeading::FromAndroidRotationVector(
				Fixture.RotationVector,
				HeadingDegrees
			));
		TestTrue(FString::Printf(TEXT("%s heading matches"), Fixture.Name),
			FMath::IsNearlyEqual(
				HeadingDegrees,
				Fixture.ExpectedHeadingDegrees,
				1.e-4
			));
	}
	double IgnoredHeading = 0.0;
	TestFalse(TEXT("A zero rotation vector is rejected"),
		FOpenMobileSensorHeading::FromAndroidRotationVector(
			FQuat(0.0, 0.0, 0.0, 0.0),
			IgnoredHeading
		));
	TestFalse(TEXT("A vertical device with no horizontal top is rejected"),
		FOpenMobileSensorHeading::FromAndroidRotationVector(
			FQuat(0.70710678, 0.0, 0.0, 0.70710678),
			IgnoredHeading
		));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMagneticHeadingScreenRotationTest,
	"OpenMobile.Sensors.Heading.Magnetic.ScreenRotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMagneticHeadingScreenRotationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	struct FFixture
	{
		EOpenMobileSensorScreenRotation Rotation;
		double ExpectedHeadingDegrees;
	};
	const FFixture Fixtures[] = {
		{EOpenMobileSensorScreenRotation::Rotation0, 350.0},
		{EOpenMobileSensorScreenRotation::Rotation90, 80.0},
		{EOpenMobileSensorScreenRotation::Rotation180, 170.0},
		{EOpenMobileSensorScreenRotation::Rotation270, 260.0}
	};
	for (const FFixture& Fixture : Fixtures)
	{
		FOpenMobileSensorsScreenRotationService::ResetForTests();
		const FGuid Owner = FGuid::NewGuid();
		TestTrue(TEXT("Screen rotation fixture is captured"),
			FOpenMobileSensorsScreenRotationService::
				CaptureApplicationWindowRotation(
					Owner,
					Fixture.Rotation,
					1.0,
					false
				));
		FOpenMobileHeadingSensorSample Sample;
		Sample.Header.TimestampSeconds = 2.0;
		Sample.Header.bValid = true;
		Sample.HeadingDegrees = 350.0;
		Sample.Reference = EOpenMobileHeadingReference::MagneticNorth;
		Sample.bTiltCompensated = true;
		FOpenMobileSensorsScreenRotationService::ApplyToSample(
			Owner,
			EOpenMobileSensorCoordinateSpace::CurrentScreen,
			Sample
		);
		TestEqual(TEXT("Heading follows the selected screen top"),
			Sample.HeadingDegrees,
			Fixture.ExpectedHeadingDegrees);
		TestEqual(TEXT("Screen rotation preserves magnetic reference"),
			Sample.Reference,
			EOpenMobileHeadingReference::MagneticNorth);
		TestTrue(TEXT("Screen rotation preserves tilt compensation"),
			Sample.bTiltCompensated);
	}
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMagneticHeadingInterferenceQualityTest,
	"OpenMobile.Sensors.Heading.Magnetic.InterferenceQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMagneticHeadingInterferenceQualityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorIdentifier Sensor;
	Sensor.Type = EOpenMobileSensorType::MagneticHeading;
	Sensor.InstanceId = TEXT("Default");
	const FOpenMobileSensorAccuracySnapshot Interference =
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor,
			-1,
			1.0
		);
	TestEqual(TEXT("Magnetic interference is unreliable"),
		Interference.Accuracy,
		EOpenMobileSensorAccuracy::Unreliable);
	TestTrue(TEXT("Uncalibrated magnetic input requests calibration"),
		Interference.bCalibrationRequired);
	const FOpenMobileSensorAccuracySnapshot Recovery =
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor,
			2,
			2.0
		);
	TestEqual(TEXT("Recovered magnetic quality is high"),
		Recovery.Accuracy,
		EOpenMobileSensorAccuracy::High);
	TestFalse(TEXT("Recovered magnetic input clears calibration"),
		Recovery.bCalibrationRequired);
	return true;
}

#endif

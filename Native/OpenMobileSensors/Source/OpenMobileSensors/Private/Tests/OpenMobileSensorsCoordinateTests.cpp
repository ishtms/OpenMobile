#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorCoordinates.h"

namespace OpenMobileSensorsCoordinateTestsPrivate
{
	bool QuaternionsRepresentSameRotation(const FQuat& A, const FQuat& B)
	{
		return FMath::Abs(A.GetNormalized() | B.GetNormalized()) > 1.0 - 1.e-9;
	}

	FOpenMobileVectorSensorSample MakeVector(
		EOpenMobileSensorType SensorType,
		const FVector& Value
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor.Type = SensorType;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = 1.0;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Value = Value;
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCoordinateAxisModelTest,
	"OpenMobile.Sensors.Coordinates.AxisModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCoordinateAxisModelTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		TestEqual(TEXT("Native right maps to device right"),
			FOpenMobileSensorCoordinateConverter::ToDevicePolarVector(
				Platform, FVector::ForwardVector),
			FVector::RightVector);
		TestEqual(TEXT("Native top maps to device top"),
			FOpenMobileSensorCoordinateConverter::ToDevicePolarVector(
				Platform, FVector::RightVector),
			FVector::ForwardVector);
		TestEqual(TEXT("Native display-out remains device display-out"),
			FOpenMobileSensorCoordinateConverter::ToDevicePolarVector(
				Platform, FVector::UpVector),
			FVector::UpVector);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCoordinateGravityTest,
	"OpenMobile.Sensors.Coordinates.Gravity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCoordinateGravityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCoordinateTestsPrivate;
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		FOpenMobileVectorSensorSample Gravity = MakeVector(
			EOpenMobileSensorType::Gravity,
			FVector(0.0, -9.80665, 0.0)
		);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			Platform, Gravity);
		TestEqual(TEXT("Gravity toward the natural bottom is negative X"),
			Gravity.Value, FVector(-9.80665, 0.0, 0.0));
		TestEqual(TEXT("Converted vectors are device-fixed"),
			Gravity.Header.CoordinateSpace,
			EOpenMobileSensorCoordinateSpace::DeviceFixed);
		TestTrue(TEXT("The header marks coordinate normalization"),
			Gravity.Header.bCoordinatesNormalized);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCoordinateAngularVelocityTest,
	"OpenMobile.Sensors.Coordinates.AngularVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCoordinateAngularVelocityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCoordinateTestsPrivate;
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		FOpenMobileVectorSensorSample AngularVelocity = MakeVector(
			EOpenMobileSensorType::Gyroscope,
			FVector(0.0, 0.0, 1.0)
		);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			Platform, AngularVelocity);
		TestEqual(TEXT("Axial Z changes sign across the reflected basis"),
			AngularVelocity.Value, FVector(0.0, 0.0, -1.0));
		TestEqual(TEXT("A native axial X maps to negative device Y"),
			FOpenMobileSensorCoordinateConverter::ToDeviceAxialVector(
				Platform, FVector::ForwardVector),
			-FVector::RightVector);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCoordinateIdentityTest,
	"OpenMobile.Sensors.Coordinates.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCoordinateIdentityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCoordinateTestsPrivate;
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		FOpenMobileAttitudeSensorSample Attitude;
		Attitude.Header.Sensor.Type = EOpenMobileSensorType::Attitude;
		Attitude.Header.bValid = true;
		Attitude.Header.bUnitsNormalized = true;
		Attitude.Quaternion = FQuat::Identity;
		Attitude.bHasEulerDegrees = true;
		Attitude.bHasRotationMatrix = true;
		FOpenMobileSensorCoordinateConverter::ConvertAttitudeSample(
			Platform, Attitude);
		TestTrue(TEXT("Identity quaternion remains identity"),
			QuaternionsRepresentSameRotation(
				Attitude.Quaternion, FQuat::Identity));
		TestEqual(TEXT("Identity matrix X column remains X"),
			Attitude.RotationMatrix.XAxis, FVector::ForwardVector);
		TestEqual(TEXT("Identity matrix Y column remains Y"),
			Attitude.RotationMatrix.YAxis, FVector::RightVector);
		TestEqual(TEXT("Identity matrix Z column remains Z"),
			Attitude.RotationMatrix.ZAxis, FVector::UpVector);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCoordinateQuarterTurnsTest,
	"OpenMobile.Sensors.Coordinates.QuarterTurns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCoordinateQuarterTurnsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCoordinateTestsPrivate;
	const double QuarterTurnRadians = UE_DOUBLE_PI * 0.5;
	const FVector NativeAxes[] = {
		FVector::ForwardVector,
		FVector::RightVector,
		FVector::UpVector
	};
	const FVector DeviceAxes[] = {
		-FVector::RightVector,
		-FVector::ForwardVector,
		-FVector::UpVector
	};
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(NativeAxes); ++Index)
		{
			FOpenMobileAttitudeSensorSample Attitude;
			Attitude.Header.Sensor.Type = EOpenMobileSensorType::Attitude;
			Attitude.Header.bValid = true;
			Attitude.Header.bUnitsNormalized = true;
			Attitude.Quaternion = FQuat(
				NativeAxes[Index], QuarterTurnRadians);
			Attitude.bHasEulerDegrees = true;
			Attitude.bHasRotationMatrix = true;
			FOpenMobileSensorCoordinateConverter::ConvertAttitudeSample(
				Platform, Attitude);
			const FQuat Expected(DeviceAxes[Index], QuarterTurnRadians);
			TestTrue(TEXT("A quarter turn uses the reflected axial axis"),
				QuaternionsRepresentSameRotation(
					Attitude.Quaternion, Expected));
			TestTrue(TEXT("Euler output matches the converted quaternion"),
				QuaternionsRepresentSameRotation(
					Attitude.EulerDegrees.Quaternion(),
					Attitude.Quaternion));
			TestEqual(TEXT("Matrix X column matches the quaternion"),
				Attitude.RotationMatrix.XAxis,
				Attitude.Quaternion.RotateVector(FVector::ForwardVector));
			TestEqual(TEXT("Matrix Y column matches the quaternion"),
				Attitude.RotationMatrix.YAxis,
				Attitude.Quaternion.RotateVector(FVector::RightVector));
			TestEqual(TEXT("Matrix Z column matches the quaternion"),
				Attitude.RotationMatrix.ZAxis,
				Attitude.Quaternion.RotateVector(FVector::UpVector));
		}
	}
	return true;
}

#endif

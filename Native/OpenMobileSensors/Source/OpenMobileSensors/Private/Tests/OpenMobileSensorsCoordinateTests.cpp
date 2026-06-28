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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeRepresentationConversionTest,
	"OpenMobile.Sensors.Attitude.Representations.GoldenConversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeRepresentationConversionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCoordinateTestsPrivate;
	const FQuat Rotations[] = {
		FQuat::Identity,
		FRotator(30.0, 45.0, 60.0).Quaternion(),
		FRotator(90.0, 25.0, -35.0).Quaternion(),
		FRotator(-90.0, -80.0, 15.0).Quaternion(),
		FRotator(0.0, 180.0, 0.0).Quaternion()
	};
	bool bNormalized = true;
	bool bEulerInRange = true;
	bool bEulerRoundTrips = true;
	bool bMatrixMatchesQuaternion = true;
	bool bMatrixOrthonormal = true;
	bool bPositiveDeterminant = true;
	bool bSignEquivalent = true;
	for (const FQuat& Rotation : Rotations)
	{
		FOpenMobileAttitudeSensorSample Sample;
		Sample.Quaternion = FQuat(
			Rotation.X * 2.0,
			Rotation.Y * 2.0,
			Rotation.Z * 2.0,
			Rotation.W * 2.0
		);
		Sample.bHasEulerDegrees = true;
		Sample.bHasRotationMatrix = true;
		FOpenMobileSensorCoordinateConverter::UpdateEulerAndRotationMatrix(
			Sample
		);
		bNormalized &= FMath::IsNearlyEqual(
			Sample.Quaternion.SizeSquared(),
			1.0,
			1.e-9
		);
		bEulerInRange &= Sample.EulerDegrees.Pitch >= -90.0
			&& Sample.EulerDegrees.Pitch <= 90.0
			&& Sample.EulerDegrees.Yaw >= -180.0
			&& Sample.EulerDegrees.Yaw <= 180.0
			&& Sample.EulerDegrees.Roll >= -180.0
			&& Sample.EulerDegrees.Roll <= 180.0;
		bEulerRoundTrips &= QuaternionsRepresentSameRotation(
			Sample.EulerDegrees.Quaternion(),
			Sample.Quaternion
		);
		const FVector& X = Sample.RotationMatrix.XAxis;
		const FVector& Y = Sample.RotationMatrix.YAxis;
		const FVector& Z = Sample.RotationMatrix.ZAxis;
		bMatrixMatchesQuaternion &= X.Equals(
			Sample.Quaternion.RotateVector(FVector::ForwardVector),
			1.e-9
		) && Y.Equals(
			Sample.Quaternion.RotateVector(FVector::RightVector),
			1.e-9
		) && Z.Equals(
			Sample.Quaternion.RotateVector(FVector::UpVector),
			1.e-9
		);
		bMatrixOrthonormal &= FMath::IsNearlyEqual(X.SizeSquared(), 1.0, 1.e-9)
			&& FMath::IsNearlyEqual(Y.SizeSquared(), 1.0, 1.e-9)
			&& FMath::IsNearlyEqual(Z.SizeSquared(), 1.0, 1.e-9)
			&& FMath::IsNearlyZero(X | Y, 1.e-9)
			&& FMath::IsNearlyZero(X | Z, 1.e-9)
			&& FMath::IsNearlyZero(Y | Z, 1.e-9);
		bPositiveDeterminant &= FMath::IsNearlyEqual(
			X | (Y ^ Z),
			1.0,
			1.e-9
		);
		FOpenMobileAttitudeSensorSample Negated;
		Negated.Quaternion = FQuat(
			-Sample.Quaternion.X,
			-Sample.Quaternion.Y,
			-Sample.Quaternion.Z,
			-Sample.Quaternion.W
		);
		Negated.bHasEulerDegrees = true;
		Negated.bHasRotationMatrix = true;
		FOpenMobileSensorCoordinateConverter::UpdateEulerAndRotationMatrix(
			Negated
		);
		bSignEquivalent &= QuaternionsRepresentSameRotation(
			Negated.EulerDegrees.Quaternion(),
			Sample.EulerDegrees.Quaternion()
		) && Negated.RotationMatrix.XAxis.Equals(X, 1.e-9)
			&& Negated.RotationMatrix.YAxis.Equals(Y, 1.e-9)
			&& Negated.RotationMatrix.ZAxis.Equals(Z, 1.e-9);
	}
	TestTrue(TEXT("Canonical quaternions are normalized"), bNormalized);
	TestTrue(TEXT("Euler output uses documented degree ranges"), bEulerInRange);
	TestTrue(TEXT("Euler output round-trips through singular poses"),
		bEulerRoundTrips);
	TestTrue(TEXT("Matrix bases match the canonical quaternion"),
		bMatrixMatchesQuaternion);
	TestTrue(TEXT("Matrix bases remain orthonormal"), bMatrixOrthonormal);
	TestTrue(TEXT("Rotation matrices keep determinant positive one"),
		bPositiveDeterminant);
	TestTrue(TEXT("Quaternion sign leaves derived output unchanged"),
		bSignEquivalent);
	return true;
}

#endif

#include "OpenMobileSensorCoordinates.h"

namespace OpenMobileSensorCoordinatesPrivate
{
	bool IsAxialSensor(EOpenMobileSensorType SensorType)
	{
		return SensorType == EOpenMobileSensorType::Gyroscope
			|| SensorType == EOpenMobileSensorType::GyroscopeUncalibrated
			|| SensorType == EOpenMobileSensorType::Magnetometer
			|| SensorType == EOpenMobileSensorType::MagnetometerUncalibrated;
	}

	void MarkConverted(FOpenMobileSensorSampleHeader& Header, bool bValid)
	{
		Header.CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed;
		Header.bCoordinatesNormalized = true;
		Header.bValid &= bValid;
	}
}

FVector FOpenMobileSensorCoordinateConverter::ToDevicePolarVector(
	EOpenMobileSensorNativePlatform Platform,
	const FVector& NativeVector
)
{
	static_cast<void>(Platform);
	return FVector(NativeVector.Y, NativeVector.X, NativeVector.Z);
}

FVector FOpenMobileSensorCoordinateConverter::ToDeviceAxialVector(
	EOpenMobileSensorNativePlatform Platform,
	const FVector& NativeVector
)
{
	return -ToDevicePolarVector(Platform, NativeVector);
}

bool FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileVectorSensorSample& Sample
)
{
	using namespace OpenMobileSensorCoordinatesPrivate;
	if (Sample.Header.bCoordinatesNormalized)
	{
		return true;
	}
	const bool bValid = !Sample.Value.ContainsNaN()
		&& (!Sample.bHasBias || !Sample.Bias.ContainsNaN());
	if (bValid)
	{
		const bool bAxial = IsAxialSensor(Sample.Header.Sensor.Type);
		Sample.Value = bAxial
			? ToDeviceAxialVector(Platform, Sample.Value)
			: ToDevicePolarVector(Platform, Sample.Value);
		if (Sample.bHasBias)
		{
			Sample.Bias = bAxial
				? ToDeviceAxialVector(Platform, Sample.Bias)
				: ToDevicePolarVector(Platform, Sample.Bias);
		}
	}
	MarkConverted(Sample.Header, bValid);
	return bValid;
}

FQuat FOpenMobileSensorCoordinateConverter::ConvertQuaternion(
	EOpenMobileSensorNativePlatform Platform,
	const FQuat& NativeQuaternion
)
{
	static_cast<void>(Platform);
	FQuat Converted(
		-NativeQuaternion.Y,
		-NativeQuaternion.X,
		-NativeQuaternion.Z,
		NativeQuaternion.W
	);
	Converted.Normalize();
	return Converted;
}

void FOpenMobileSensorCoordinateConverter::UpdateEulerAndRotationMatrix(
	FOpenMobileAttitudeSensorSample& Sample
)
{
	if (Sample.bHasEulerDegrees)
	{
		Sample.EulerDegrees = Sample.Quaternion.Rotator();
	}
	if (Sample.bHasRotationMatrix)
	{
		Sample.RotationMatrix.XAxis = Sample.Quaternion.RotateVector(
			FVector::ForwardVector
		);
		Sample.RotationMatrix.YAxis = Sample.Quaternion.RotateVector(
			FVector::RightVector
		);
		Sample.RotationMatrix.ZAxis = Sample.Quaternion.RotateVector(
			FVector::UpVector
		);
	}
}

bool FOpenMobileSensorCoordinateConverter::ConvertAttitudeSample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileAttitudeSensorSample& Sample
)
{
	using namespace OpenMobileSensorCoordinatesPrivate;
	if (Sample.Header.bCoordinatesNormalized)
	{
		return true;
	}
	const bool bValid = !Sample.Quaternion.ContainsNaN()
		&& Sample.Quaternion.SizeSquared() > UE_DOUBLE_SMALL_NUMBER;
	if (bValid)
	{
		Sample.Quaternion = ConvertQuaternion(Platform, Sample.Quaternion);
		UpdateEulerAndRotationMatrix(Sample);
	}
	MarkConverted(Sample.Header, bValid);
	return bValid;
}

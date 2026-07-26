#include "OpenMobileSensorIdentifiers.h"

const TArray<EOpenMobileSensorType>& FOpenMobileSensorTypes::GetAll()
{
	static const TArray<EOpenMobileSensorType> Types = {
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorType::AccelerometerUncalibrated,
		EOpenMobileSensorType::Gyroscope,
		EOpenMobileSensorType::GyroscopeUncalibrated,
		EOpenMobileSensorType::Magnetometer,
		EOpenMobileSensorType::MagnetometerUncalibrated,
		EOpenMobileSensorType::Gravity,
		EOpenMobileSensorType::LinearAcceleration,
		EOpenMobileSensorType::Attitude,
		EOpenMobileSensorType::MagneticHeading,
		EOpenMobileSensorType::TrueHeading,
		EOpenMobileSensorType::BarometricPressure,
		EOpenMobileSensorType::RelativeAltitude,
		EOpenMobileSensorType::AbsoluteAltitude,
		EOpenMobileSensorType::AmbientLight,
		EOpenMobileSensorType::Proximity,
		EOpenMobileSensorType::StepCounter,
		EOpenMobileSensorType::StepDetector,
		EOpenMobileSensorType::Pedometer,
		EOpenMobileSensorType::MotionActivity,
		EOpenMobileSensorType::ActivityTransition,
		EOpenMobileSensorType::PhysicalOrientation,
		EOpenMobileSensorType::Shake
	};
	return Types;
}

FName FOpenMobileSensorTypes::GetStableName(EOpenMobileSensorType Type)
{
	switch (Type)
	{
	case EOpenMobileSensorType::Accelerometer:
		return TEXT("OpenMobile.Sensors.Accelerometer");
	case EOpenMobileSensorType::AccelerometerUncalibrated:
		return TEXT("OpenMobile.Sensors.AccelerometerUncalibrated");
	case EOpenMobileSensorType::Gyroscope:
		return TEXT("OpenMobile.Sensors.Gyroscope");
	case EOpenMobileSensorType::GyroscopeUncalibrated:
		return TEXT("OpenMobile.Sensors.GyroscopeUncalibrated");
	case EOpenMobileSensorType::Magnetometer:
		return TEXT("OpenMobile.Sensors.Magnetometer");
	case EOpenMobileSensorType::MagnetometerUncalibrated:
		return TEXT("OpenMobile.Sensors.MagnetometerUncalibrated");
	case EOpenMobileSensorType::Gravity:
		return TEXT("OpenMobile.Sensors.Gravity");
	case EOpenMobileSensorType::LinearAcceleration:
		return TEXT("OpenMobile.Sensors.LinearAcceleration");
	case EOpenMobileSensorType::Attitude:
		return TEXT("OpenMobile.Sensors.Attitude");
	case EOpenMobileSensorType::MagneticHeading:
		return TEXT("OpenMobile.Sensors.MagneticHeading");
	case EOpenMobileSensorType::TrueHeading:
		return TEXT("OpenMobile.Sensors.TrueHeading");
	case EOpenMobileSensorType::BarometricPressure:
		return TEXT("OpenMobile.Sensors.BarometricPressure");
	case EOpenMobileSensorType::RelativeAltitude:
		return TEXT("OpenMobile.Sensors.RelativeAltitude");
	case EOpenMobileSensorType::AbsoluteAltitude:
		return TEXT("OpenMobile.Sensors.AbsoluteAltitude");
	case EOpenMobileSensorType::AmbientLight:
		return TEXT("OpenMobile.Sensors.AmbientLight");
	case EOpenMobileSensorType::Proximity:
		return TEXT("OpenMobile.Sensors.Proximity");
	case EOpenMobileSensorType::StepCounter:
		return TEXT("OpenMobile.Sensors.StepCounter");
	case EOpenMobileSensorType::StepDetector:
		return TEXT("OpenMobile.Sensors.StepDetector");
	case EOpenMobileSensorType::Pedometer:
		return TEXT("OpenMobile.Sensors.Pedometer");
	case EOpenMobileSensorType::MotionActivity:
		return TEXT("OpenMobile.Sensors.MotionActivity");
	case EOpenMobileSensorType::ActivityTransition:
		return TEXT("OpenMobile.Sensors.ActivityTransition");
	case EOpenMobileSensorType::PhysicalOrientation:
		return TEXT("OpenMobile.Sensors.PhysicalOrientation");
	case EOpenMobileSensorType::Shake:
		return TEXT("OpenMobile.Sensors.Shake");
	case EOpenMobileSensorType::Unknown:
	default:
		return NAME_None;
	}
}

EOpenMobileSensorSampleFamily FOpenMobileSensorTypes::GetSampleFamily(
	EOpenMobileSensorType Type
)
{
	switch (Type)
	{
	case EOpenMobileSensorType::Accelerometer:
	case EOpenMobileSensorType::AccelerometerUncalibrated:
	case EOpenMobileSensorType::Gyroscope:
	case EOpenMobileSensorType::GyroscopeUncalibrated:
	case EOpenMobileSensorType::Magnetometer:
	case EOpenMobileSensorType::MagnetometerUncalibrated:
	case EOpenMobileSensorType::Gravity:
	case EOpenMobileSensorType::LinearAcceleration:
	case EOpenMobileSensorType::Shake:
		return EOpenMobileSensorSampleFamily::Vector;
	case EOpenMobileSensorType::Attitude:
		return EOpenMobileSensorSampleFamily::Attitude;
	case EOpenMobileSensorType::BarometricPressure:
	case EOpenMobileSensorType::RelativeAltitude:
	case EOpenMobileSensorType::AbsoluteAltitude:
	case EOpenMobileSensorType::AmbientLight:
		return EOpenMobileSensorSampleFamily::Scalar;
	case EOpenMobileSensorType::MagneticHeading:
	case EOpenMobileSensorType::TrueHeading:
		return EOpenMobileSensorSampleFamily::Heading;
	case EOpenMobileSensorType::StepCounter:
	case EOpenMobileSensorType::StepDetector:
	case EOpenMobileSensorType::Pedometer:
		return EOpenMobileSensorSampleFamily::Steps;
	case EOpenMobileSensorType::MotionActivity:
	case EOpenMobileSensorType::ActivityTransition:
		return EOpenMobileSensorSampleFamily::Activity;
	case EOpenMobileSensorType::PhysicalOrientation:
		return EOpenMobileSensorSampleFamily::Orientation;
	case EOpenMobileSensorType::Proximity:
		return EOpenMobileSensorSampleFamily::Proximity;
	case EOpenMobileSensorType::Unknown:
	default:
		return EOpenMobileSensorSampleFamily::Unknown;
	}
}

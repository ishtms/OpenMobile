#include "OpenMobileSensorSourcePolicy.h"

namespace OpenMobileSensorSourcePolicyPrivate
{
	constexpr int32 Flag(EOpenMobileSensorSourceFlags Value)
	{
		return static_cast<int32>(Value);
	}

	constexpr int32 KnownFlags =
		Flag(EOpenMobileSensorSourceFlags::Raw)
		| Flag(EOpenMobileSensorSourceFlags::CalibratedNative)
		| Flag(EOpenMobileSensorSourceFlags::NativeFused)
		| Flag(EOpenMobileSensorSourceFlags::PluginDerived)
		| Flag(EOpenMobileSensorSourceFlags::MagneticNorthReferenced)
		| Flag(EOpenMobileSensorSourceFlags::TrueNorthReferenced)
		| Flag(EOpenMobileSensorSourceFlags::Mock)
		| Flag(EOpenMobileSensorSourceFlags::Replay);

	int32 GetNativeFusedFlags(EOpenMobileSensorType Sensor)
	{
		int32 Flags = Flag(EOpenMobileSensorSourceFlags::NativeFused);
		if (Sensor == EOpenMobileSensorType::MagneticHeading)
		{
			Flags |= Flag(
				EOpenMobileSensorSourceFlags::MagneticNorthReferenced
			);
		}
		else if (Sensor == EOpenMobileSensorType::TrueHeading)
		{
			Flags |= Flag(
				EOpenMobileSensorSourceFlags::TrueNorthReferenced
			);
		}
		return Flags;
	}

	bool IsNativeFused(EOpenMobileSensorType Sensor)
	{
		return Sensor == EOpenMobileSensorType::Gravity
			|| Sensor == EOpenMobileSensorType::LinearAcceleration
			|| Sensor == EOpenMobileSensorType::Attitude
			|| Sensor == EOpenMobileSensorType::RelativeAltitude
			|| Sensor == EOpenMobileSensorType::AbsoluteAltitude
			|| Sensor == EOpenMobileSensorType::MotionActivity
			|| Sensor == EOpenMobileSensorType::MagneticHeading
			|| Sensor == EOpenMobileSensorType::TrueHeading;
	}

	bool IsUncalibrated(EOpenMobileSensorType Sensor)
	{
		return Sensor == EOpenMobileSensorType::AccelerometerUncalibrated
			|| Sensor == EOpenMobileSensorType::GyroscopeUncalibrated
			|| Sensor == EOpenMobileSensorType::MagnetometerUncalibrated;
	}

	bool IsCalibratedAndroidMotion(EOpenMobileSensorType Sensor)
	{
		return Sensor == EOpenMobileSensorType::Accelerometer
			|| Sensor == EOpenMobileSensorType::Gyroscope
			|| Sensor == EOpenMobileSensorType::Magnetometer;
	}
}

bool FOpenMobileSensorSourcePolicy::ValidateSourceFlags(int32 SourceFlags)
{
	using namespace OpenMobileSensorSourcePolicyPrivate;
	if (SourceFlags < 0 || (SourceFlags & ~KnownFlags) != 0)
	{
		return false;
	}
	const bool bRaw =
		(SourceFlags & Flag(EOpenMobileSensorSourceFlags::Raw)) != 0;
	const bool bCalibrated =
		(SourceFlags
			& Flag(EOpenMobileSensorSourceFlags::CalibratedNative)) != 0;
	const bool bNativeFused =
		(SourceFlags & Flag(EOpenMobileSensorSourceFlags::NativeFused)) != 0;
	const bool bPluginDerived =
		(SourceFlags & Flag(EOpenMobileSensorSourceFlags::PluginDerived)) != 0;
	if ((bRaw && (bCalibrated || bNativeFused || bPluginDerived))
		|| (bPluginDerived && (bCalibrated || bNativeFused)))
	{
		return false;
	}
	const bool bMagneticNorth =
		(SourceFlags
			& Flag(EOpenMobileSensorSourceFlags::MagneticNorthReferenced)) != 0;
	const bool bTrueNorth =
		(SourceFlags
			& Flag(EOpenMobileSensorSourceFlags::TrueNorthReferenced)) != 0;
	return !(bMagneticNorth && bTrueNorth)
		&& !((bMagneticNorth || bTrueNorth)
			&& (bRaw || (!bNativeFused && !bPluginDerived)));
}

int32 FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
	EOpenMobileSensorType Sensor
)
{
	using namespace OpenMobileSensorSourcePolicyPrivate;
	if (Sensor == EOpenMobileSensorType::Unknown)
	{
		return 0;
	}
	if (IsNativeFused(Sensor))
	{
		return GetNativeFusedFlags(Sensor);
	}
	if (IsUncalibrated(Sensor))
	{
		return Flag(EOpenMobileSensorSourceFlags::Raw);
	}
	if (IsCalibratedAndroidMotion(Sensor))
	{
		return Flag(EOpenMobileSensorSourceFlags::CalibratedNative);
	}
	return Flag(EOpenMobileSensorSourceFlags::Raw);
}

int32 FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
	EOpenMobileSensorType Sensor
)
{
	using namespace OpenMobileSensorSourcePolicyPrivate;
	if (Sensor == EOpenMobileSensorType::Unknown)
	{
		return 0;
	}
	if (IsNativeFused(Sensor))
	{
		return GetNativeFusedFlags(Sensor);
	}
	return Flag(EOpenMobileSensorSourceFlags::Raw);
}

void FOpenMobileSensorSourcePolicy::MarkReplayed(
	FOpenMobileSensorSampleHeader& Header
)
{
	Header.SourceFlags |= static_cast<int32>(
		EOpenMobileSensorSourceFlags::Replay
	);
}

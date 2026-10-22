#pragma once

#include "CoreMinimal.h"

class OPENMOBILESENSORS_API FOpenMobileSensorTimestampConverter final
{
public:
	static double FromAndroidSensorEventNanoseconds(
		int64 TimestampNanoseconds
	);
	static double FromIOSCoreMotionSeconds(double TimestampSeconds);
	/** Preserves acquisition age and batch spacing while changing the clock epoch. */
	static double ToPlatformSeconds(
		double NativeTimestampSeconds,
		double NativeNowSeconds,
		double PlatformNowSeconds
	);
};

#pragma once

#include "CoreMinimal.h"

class OPENMOBILESENSORS_API FOpenMobileSensorTimestampConverter final
{
public:
	static double FromAndroidSensorEventNanoseconds(
		int64 TimestampNanoseconds
	);
	static double FromIOSCoreMotionSeconds(double TimestampSeconds);
};

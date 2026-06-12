#include "OpenMobileSensorTimestamp.h"

double FOpenMobileSensorTimestampConverter::
FromAndroidSensorEventNanoseconds(int64 TimestampNanoseconds)
{
	return static_cast<double>(TimestampNanoseconds) * 1.e-9;
}

double FOpenMobileSensorTimestampConverter::FromIOSCoreMotionSeconds(
	double TimestampSeconds
)
{
	return TimestampSeconds;
}

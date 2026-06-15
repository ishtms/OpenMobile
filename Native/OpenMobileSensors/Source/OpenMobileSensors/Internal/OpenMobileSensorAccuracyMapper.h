#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorAccuracy.h"

class OPENMOBILESENSORS_API FOpenMobileSensorAccuracyMapper final
{
public:
	static FOpenMobileSensorAccuracySnapshot FromAndroidAccuracyCallback(
		const FOpenMobileSensorIdentifier& Sensor,
		int32 NativeAccuracy,
		double TimestampSeconds
	);
	static FOpenMobileSensorAccuracySnapshot FromIOSMagneticFieldAccuracy(
		const FOpenMobileSensorIdentifier& Sensor,
		int32 NativeAccuracy,
		double TimestampSeconds
	);
	static FOpenMobileSensorAccuracySnapshot FromIOSHeadingAccuracy(
		const FOpenMobileSensorIdentifier& Sensor,
		double AccuracyDegrees,
		bool bCalibrationRequired,
		double TimestampSeconds
	);
};

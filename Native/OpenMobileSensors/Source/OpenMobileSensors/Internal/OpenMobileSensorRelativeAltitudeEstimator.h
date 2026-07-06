#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorRelativeAltitudeEstimator final
{
public:
	bool Process(
		const FOpenMobileScalarSensorSample& Input,
		const FOpenMobileSensorIdentifier& OutputSensor,
		FOpenMobileScalarSensorSample& OutSample
	);
	void Reset();

private:
	EOpenMobileRelativeAltitudeSource BaselineSource =
		EOpenMobileRelativeAltitudeSource::None;
	double BaselineValue = 0.0;
	double BaselineTimestampSeconds = 0.0;
	bool bHasBaseline = false;
};

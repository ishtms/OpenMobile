#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorGravityEstimator.h"

class OPENMOBILESENSORS_API FOpenMobileSensorLinearAccelerationEstimator final
{
public:
	bool Process(
		const FOpenMobileVectorSensorSample& Acceleration,
		const FOpenMobileSensorIdentifier& LinearAccelerationSensor,
		FOpenMobileVectorSensorSample& OutLinearAcceleration
	);
	void Reset();

private:
	FOpenMobileSensorGravityEstimator GravityEstimator;
};

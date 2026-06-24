#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorGravityEstimator final
{
public:
	bool Process(
		const FOpenMobileVectorSensorSample& Acceleration,
		const FOpenMobileSensorIdentifier& GravitySensor,
		FOpenMobileVectorSensorSample& OutGravity
	);
	void Reset();

private:
	FVector Estimate = FVector::ZeroVector;
	double LastTimestampSeconds = 0.0;
	int32 LastSourceFlags = 0;
	bool bHasEstimate = false;
};

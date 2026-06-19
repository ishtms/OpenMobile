#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

struct FOpenMobileSensorFilterOptions;

class OPENMOBILESENSORS_API FOpenMobileSensorVectorFilter final
{
public:
	bool Apply(
		const FOpenMobileSensorFilterOptions& Options,
		FOpenMobileVectorSensorSample& Sample
	);
	void Reset();

private:
	FVector LowPassState = FVector::ZeroVector;
	FVector HighPassPreviousInput = FVector::ZeroVector;
	FVector HighPassState = FVector::ZeroVector;
	FVector SmoothingState = FVector::ZeroVector;
	double LastTimestampSeconds = 0.0;
	bool bInitialized = false;
};

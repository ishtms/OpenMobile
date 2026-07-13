#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

struct FOpenMobileSensorFilterOptions;

class OPENMOBILESENSORS_API FOpenMobileSensorHeadingFilter final
{
public:
	bool Apply(
		const FOpenMobileSensorFilterOptions& Options,
		FOpenMobileHeadingSensorSample& Sample
	);
	void Reset();

private:
	double SmoothedHeadingDegrees = 0.0;
	double LastTimestampSeconds = 0.0;
	bool bInitialized = false;
};

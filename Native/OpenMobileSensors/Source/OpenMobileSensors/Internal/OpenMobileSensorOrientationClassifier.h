#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

struct FOpenMobileSensorOrientationClassifierConfig
{
	double FaceAngleDegrees = 25.0;
	double HysteresisDegrees = 5.0;
	double TransitionDebounceSeconds = 0.15;
};

class OPENMOBILESENSORS_API FOpenMobileSensorOrientationClassifier final
{
public:
	bool Process(
		const FVector& Gravity,
		double TimestampSeconds,
		const FOpenMobileSensorOrientationClassifierConfig& Config,
		FOpenMobileOrientationSensorSample& OutOrientation
	);
	void Reset();

private:
	EOpenMobilePhysicalOrientation Current =
		EOpenMobilePhysicalOrientation::Unknown;
	EOpenMobilePhysicalOrientation Pending =
		EOpenMobilePhysicalOrientation::Unknown;
	double PendingSinceSeconds = 0.0;
	double LastTimestampSeconds = 0.0;
	bool bHasState = false;
	bool bHasPending = false;
};

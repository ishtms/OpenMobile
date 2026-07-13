#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorShakeDetector final
{
public:
	void Configure(const FOpenMobileShakeDetectionOptions& InOptions);
	bool Process(
		const FOpenMobileVectorSensorSample& Sample,
		FOpenMobileVectorSensorSample& OutEvent
	);
	void Reset();

private:
	struct FImpulse
	{
		double TimestampSeconds = 0.0;
		double StrengthMetresPerSecondSquared = 0.0;
	};

	FOpenMobileShakeDetectionOptions Options;
	TArray<FImpulse> Impulses;
	FOpenMobileSensorIdentifier LastSourceSensor;
	double LastTimestampSeconds = 0.0;
	double QuietSinceSeconds = 0.0;
	double CooldownUntilSeconds = 0.0;
	int32 LastSourceFlags = 0;
	bool bHasTimestamp = false;
	bool bHasSource = false;
	bool bQuietPeriodActive = false;
	bool bArmed = true;
};

#pragma once

#include "OpenMobileSensorSamples.h"

class FOpenMobileStepDetectionTracker
{
public:
	void Initialize(const FGuid& InSessionIdentifier);

	bool Process(
		const FOpenMobileStepsSensorSample& Input,
		FOpenMobileStepsSensorSample& OutEvent
	);

private:
	static int64 SaturatingAdd(
		int64 Left,
		int64 Right,
		bool& bOutSaturated
	);

	FGuid SessionIdentifier;
	FGuid PedometerOriginIdentifier;
	int64 SessionCount = 0;
	int64 LastPedometerTotal = 0;
	double LastDirectTimestampSeconds = 0.0;
	double LastPedometerQueryEnd = 0.0;
	bool bCountSaturated = false;
	bool bHasDirectTimestamp = false;
	bool bHasPedometerState = false;
};

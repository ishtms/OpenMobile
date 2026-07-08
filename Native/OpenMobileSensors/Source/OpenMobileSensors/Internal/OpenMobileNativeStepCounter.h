#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileNativeStepCounterTracker final
{
public:
	bool Apply(FOpenMobileStepsSensorSample& Sample);
	void Reset();

	static bool TryConvertNativeTotal(double NativeTotal, int64& OutTotal);

private:
	FGuid OriginIdentifier;
	EOpenMobileStepCountOrigin Origin = EOpenMobileStepCountOrigin::Unknown;
	int64 LastCount = 0;
	double QueryStartUnixTimeSeconds = 0.0;
	bool bHasQueryInterval = false;
	bool bHasSample = false;
};

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorHeadingQuality final
{
public:
	static void Normalize(FOpenMobileHeadingSensorSample& Sample);
	static bool MeetsMinimum(
		EOpenMobileSensorAccuracy Accuracy,
		EOpenMobileSensorAccuracy MinimumAccuracy
	);
};

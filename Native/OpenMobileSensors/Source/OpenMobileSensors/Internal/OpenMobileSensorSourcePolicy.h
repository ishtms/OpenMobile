#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorSourcePolicy final
{
public:
	static bool ValidateSourceFlags(int32 SourceFlags);
	static int32 GetAndroidNativeSourceFlags(
		EOpenMobileSensorType Sensor
	);
	static int32 GetIOSNativeSourceFlags(EOpenMobileSensorType Sensor);
	static void MarkReplayed(FOpenMobileSensorSampleHeader& Header);
};

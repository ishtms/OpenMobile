#pragma once

#include "CoreMinimal.h"

class OPENMOBILESENSORS_API FOpenMobileSensorHeading
{
public:
	static bool FromAndroidRotationVector(
		const FQuat& RotationVector,
		double& OutHeadingDegrees
	);
};

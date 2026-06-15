#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorValidity final
{
public:
	static bool IsEligibleForStatefulProcessing(
		const FOpenMobileVectorSensorSample& Sample
	);
	static bool IsEligibleForStatefulProcessing(
		const FOpenMobileAttitudeSensorSample& Sample
	);
	static bool IsEligibleForStatefulProcessing(
		const FOpenMobileScalarSensorSample& Sample
	);
	static bool IsEligibleForStatefulProcessing(
		const FOpenMobileHeadingSensorSample& Sample
	);
	static bool IsEligibleForStatefulProcessing(
		const FOpenMobileStepsSensorSample& Sample
	);
	static bool IsEligibleForStatefulProcessing(
		const FOpenMobileActivitySensorSample& Sample
	);
	static bool IsEligibleForStatefulProcessing(
		const FOpenMobileOrientationSensorSample& Sample
	);
	static bool IsEligibleForStatefulProcessing(
		const FOpenMobileProximitySensorSample& Sample
	);
};

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

enum class EOpenMobileSensorNativePlatform : uint8
{
	Android,
	IOS
};

struct FOpenMobileSensorNativeUnitDiagnostics
{
#if !UE_BUILD_SHIPPING
	double SanitizedValues[4] = {};
	int32 SanitizedValueCount = 0;
#endif
};

class OPENMOBILESENSORS_API FOpenMobileSensorUnitConverter final
{
public:
	static bool NormalizeVectorSample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileVectorSensorSample& Sample,
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics = nullptr
	);
	static bool NormalizeAttitudeSample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileAttitudeSensorSample& Sample,
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics = nullptr
	);
	static bool NormalizeScalarSample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileScalarSensorSample& Sample,
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics = nullptr
	);
	static bool NormalizeHeadingSample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileHeadingSensorSample& Sample,
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics = nullptr
	);
	static bool NormalizeStepsSample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileStepsSensorSample& Sample,
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics = nullptr
	);
	static bool NormalizeActivitySample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileActivitySensorSample& Sample,
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics = nullptr
	);
	static bool NormalizeOrientationSample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileOrientationSensorSample& Sample,
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics = nullptr
	);
	static bool NormalizeProximitySample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileProximitySensorSample& Sample,
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics = nullptr
	);

	static double SanitizeNativeValue(double Value);
};

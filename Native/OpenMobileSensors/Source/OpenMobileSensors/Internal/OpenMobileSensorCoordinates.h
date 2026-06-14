#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorUnits.h"

class OPENMOBILESENSORS_API FOpenMobileSensorCoordinateConverter final
{
public:
	static FVector ToDevicePolarVector(
		EOpenMobileSensorNativePlatform Platform,
		const FVector& NativeVector
	);
	static FVector ToDeviceAxialVector(
		EOpenMobileSensorNativePlatform Platform,
		const FVector& NativeVector
	);
	static bool ConvertVectorSample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileVectorSensorSample& Sample
	);
	static bool ConvertAttitudeSample(
		EOpenMobileSensorNativePlatform Platform,
		FOpenMobileAttitudeSensorSample& Sample
	);
	static void UpdateEulerAndRotationMatrix(
		FOpenMobileAttitudeSensorSample& Sample
	);

private:
	static FQuat ConvertQuaternion(
		EOpenMobileSensorNativePlatform Platform,
		const FQuat& NativeQuaternion
	);
};

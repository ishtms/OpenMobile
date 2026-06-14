#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileSensorScreenRotation.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsScreenRotationService final
{
public:
	static bool CaptureApplicationWindowRotation(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorScreenRotation Rotation,
		double TimestampSeconds,
		bool bNaturalOrientationLandscape
	);
	static bool ResolveRotation(
		const FGuid& OwnerIdentifier,
		double SampleTimestampSeconds,
		FOpenMobileSensorScreenRotationSnapshot& OutSnapshot
	);
	static void RemoveOwner(const FGuid& OwnerIdentifier);
	static void Reset();

	static FVector RotateVector(
		const FVector& DeviceVector,
		EOpenMobileSensorScreenRotation Rotation
	);
	static void ApplyToSample(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		FOpenMobileVectorSensorSample& Sample
	);
	static void ApplyToSample(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		FOpenMobileAttitudeSensorSample& Sample
	);
	static void ApplyToSample(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		FOpenMobileScalarSensorSample& Sample
	);
	static void ApplyToSample(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		FOpenMobileHeadingSensorSample& Sample
	);
	static void ApplyToSample(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		FOpenMobileStepsSensorSample& Sample
	);
	static void ApplyToSample(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		FOpenMobileActivitySensorSample& Sample
	);
	static void ApplyToSample(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		FOpenMobileOrientationSensorSample& Sample
	);
	static void ApplyToSample(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		FOpenMobileProximitySensorSample& Sample
	);

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests() { Reset(); }
#endif
};

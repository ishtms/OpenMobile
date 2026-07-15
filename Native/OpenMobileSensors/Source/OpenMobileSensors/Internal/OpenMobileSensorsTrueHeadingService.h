#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorErrors.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsTrueHeadingService final
{
public:
	static EOpenMobileSensorFailureReason SetLocationInput(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorLocationInput& LocationInput,
		double CurrentUnixSeconds,
		double CurrentMonotonicSeconds
	);
	static EOpenMobileSensorFailureReason GetUsableLocationInput(
		const FGuid& OwnerIdentifier,
		double CurrentMonotonicSeconds,
		FOpenMobileSensorLocationInput& OutLocationInput,
		double& OutLocationAgeSeconds
	);
	static EOpenMobileSensorFailureReason GetLocationInputState(
		const FGuid& OwnerIdentifier,
		double CurrentMonotonicSeconds
	);
	static EOpenMobileSensorFailureReason ConvertMagneticHeading(
		const FGuid& OwnerIdentifier,
		double CurrentMonotonicSeconds,
		const FOpenMobileHeadingSensorSample& MagneticHeading,
		FOpenMobileHeadingSensorSample& OutTrueHeading
	);
	static EOpenMobileSensorFailureReason AnnotateNativeHeading(
		const FGuid& OwnerIdentifier,
		double CurrentMonotonicSeconds,
		FOpenMobileHeadingSensorSample& NativeHeading
	);
	static void RemoveOwner(const FGuid& OwnerIdentifier);
	static bool ClearLocationInput(const FGuid& OwnerIdentifier);
	static bool HasAnyLocationInput(double CurrentMonotonicSeconds);
	static double GetMaximumLocationAgeSeconds();
	static double GetMaximumHorizontalAccuracyMeters();

#if WITH_DEV_AUTOMATION_TESTS
	static bool HasRetainedLocationInputForTests(
		const FGuid& OwnerIdentifier
	);
	static void ResetForTests();
#endif
};

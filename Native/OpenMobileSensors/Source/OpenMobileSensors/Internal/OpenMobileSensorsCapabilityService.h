#pragma once

#include "CoreMinimal.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorCapabilities.h"

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorCapabilityMatrixChanged,
	const FOpenMobileSensorCapabilitySnapshot&
);

class OPENMOBILESENSORS_API FOpenMobileSensorsCapabilityService final
{
public:
	static void Start();
	static void BeginShutdown();
	static FOpenMobileSensorCapabilitySnapshot GetSnapshot();
	static void HandleBackendGenerationChanged();
	static void NotifyPermissionStatusChanged(
		FName Permission,
		EOpenMobilePermissionStatus Status
	);
	static FOpenMobilePermissionResult RefreshPermissionStatus(
		FName Permission
	);
	static void ApplyTrueHeadingLocationInputState(
		FOpenMobileSensorCapabilitySnapshot& Snapshot,
		EOpenMobileSensorFailureReason InputState
	);
	static void SetApplicationActive(bool bActive);
	static void SetLocationInputAvailable(bool bAvailable);
	static FOnOpenMobileSensorCapabilityMatrixChanged& OnChanged();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};

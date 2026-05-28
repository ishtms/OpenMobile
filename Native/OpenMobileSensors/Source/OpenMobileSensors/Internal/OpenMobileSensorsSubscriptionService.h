#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorResults.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsSubscriptionService final
{
public:
	static void Start();
	static void BeginShutdown();
	static void HandleBackendGenerationChanged();

	static FOpenMobileSensorSubscriptionResult StartSubscription(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionRequest& Request
	);
	static FOpenMobileSensorOperationResult UpdateSubscription(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorStreamOptions& Options
	);
	static FOpenMobileSensorOperationResult StopSubscription(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static int32 StopAllSubscriptions(const FGuid& OwnerIdentifier);
	static bool GetSubscriptionState(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOpenMobileSensorSubscriptionStateSnapshot& OutState
	);
	static FOpenMobileSensorOperationResult GetHandleStatus(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static bool IsHandleCurrent(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static bool IsHandleCurrent(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static void InvalidateForUnrecoverablePermissionLoss(
		EOpenMobileSensorType SensorType
	);

#if WITH_DEV_AUTOMATION_TESTS
	static int32 GetActiveSubscriptionCountForTests();
	static int32 GetActiveSubscriptionCountForTests(
		const FOpenMobileSensorIdentifier& Sensor
	);
	static void ResetForTests();
#endif
};

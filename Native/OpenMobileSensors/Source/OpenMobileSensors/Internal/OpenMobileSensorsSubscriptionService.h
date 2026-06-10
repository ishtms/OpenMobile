#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorDiagnostics.h"
#include "OpenMobileSensorResults.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileSensorSubscriptionServiceStateChanged,
	const FGuid&,
	const FOpenMobileSensorSubscriptionStateSnapshot&
);

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
	static TArray<FOpenMobileSensorStreamDiagnostics> GetStreamDiagnostics(
		const FGuid& OwnerIdentifier
	);
	static TArray<FOpenMobileSensorSubscriptionHandle>
	SelectSubscribersForSample(
		const FOpenMobileSensorIdentifier& Sensor,
		double TimestampSeconds
	);
	static void InvalidateForUnrecoverablePermissionLoss(
		EOpenMobileSensorType SensorType
	);
	static FOnOpenMobileSensorSubscriptionServiceStateChanged&
	OnStateChanged();

#if WITH_DEV_AUTOMATION_TESTS
	static int32 GetActiveSubscriptionCountForTests();
	static int32 GetActiveSubscriptionCountForTests(
		const FOpenMobileSensorIdentifier& Sensor
	);
	static int32 GetPhysicalStreamCountForTests();
	static void ProcessPendingBackendOperationsForTests();
	static void ResetForTests();
#endif
};

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorDiagnostics.h"
#include "OpenMobileSensorResults.h"

struct FOpenMobileSensorsBackendToken;
struct FOpenMobileSensorBackendStreamHandle;

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
	static FOpenMobileSensorOperationResult RecenterAttitude(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode
	);
	static FOpenMobileSensorOperationResult RecenterRelativeAltitude(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static FOpenMobileSensorOperationResult ResetStepCountSession(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static FOpenMobileSensorOperationResult RequestNativeCalibrationPrompt(
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
	static void FlushSubscription(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FGuid& RequestId,
		TFunction<void(const FOpenMobileSensorFlushResult&)>&& Completion
	);
	static bool CancelFlush(
		const FGuid& OwnerIdentifier,
		const FGuid& RequestId
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
	static bool FailPhysicalStreamFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileSensorOperationResult& Failure
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
	static void ProcessPendingBackendOperationsForTests(double NowSeconds);
	static void ProcessFlushTimeoutsForTests(double NowSeconds);
	static void SetSubscriptionStateForTests(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorSubscriptionState State
	);
	static void ResetForTests();
#endif
};

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorCalibration.h"
#include "OpenMobileSensorDiagnostics.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"

struct FOpenMobileSensorsBackendToken;
struct FOpenMobileSensorBackendStreamHandle;

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileVectorSensorBatchReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileVectorSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileAttitudeSensorBatchReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileAttitudeSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileScalarSensorBatchReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileScalarSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileHeadingSensorBatchReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileHeadingSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileStepsSensorBatchReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileStepsSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileActivitySensorBatchReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileActivitySensorBatch&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileOrientationSensorBatchReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileOrientationSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileProximitySensorBatchReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileProximitySensorBatch&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileSensorAccuracyChangedReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileSensorAccuracySnapshot&
);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOpenMobileSensorCalibrationChangedReady,
	const FGuid&,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileSensorCalibrationEvent&
);

class OPENMOBILESENSORS_API FOpenMobileSensorsSampleService final
{
public:
	static void Start();
	static void BeginShutdown();
	static void RegisterSubscription(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorIdentifier& PhysicalSensor,
		const FOpenMobileSensorStreamOptions& Options,
		uint64 BackendGeneration
	);
	static void SetPhysicalStreamHandle(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle
	);
	static void SetSubscriptionState(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorSubscriptionState State
	);
	static void UpdateSubscriptionOptions(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorStreamOptions& Options
	);
	static void UnregisterSubscription(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static void UnregisterAll();
	static bool GetRateDiagnostics(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOpenMobileSensorRateDiagnostics& OutRate
	);
	static bool GetDeliveryDiagnostics(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		double NowSeconds,
		FOpenMobileSensorStreamDiagnostics& OutDiagnostics
	);
	static bool FlushPluginSamples(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32& OutSampleCount
	);
	static bool RecenterAttitude(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode
	);
	static bool RecenterRelativeAltitude(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static bool GetAttitudeRecenterState(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOpenMobileSensorRecenterState& OutState
	);

	static void PublishVector(const FOpenMobileVectorSensorSample& Sample);
	static void PublishAttitude(const FOpenMobileAttitudeSensorSample& Sample);
	static void PublishScalar(const FOpenMobileScalarSensorSample& Sample);
	static void PublishHeading(const FOpenMobileHeadingSensorSample& Sample);
	static void PublishSteps(const FOpenMobileStepsSensorSample& Sample);
	static void PublishActivity(const FOpenMobileActivitySensorSample& Sample);
	static void PublishOrientation(
		const FOpenMobileOrientationSensorSample& Sample
	);
	static void PublishProximity(const FOpenMobileProximitySensorSample& Sample);
	static bool PublishAccuracy(
		const FOpenMobileSensorAccuracySnapshot& Snapshot
	);
	static bool PublishVectorBatch(const FOpenMobileVectorSensorBatch& Batch);
	static bool PublishAttitudeBatch(
		const FOpenMobileAttitudeSensorBatch& Batch
	);
	static bool PublishScalarBatch(const FOpenMobileScalarSensorBatch& Batch);
	static bool PublishHeadingBatch(const FOpenMobileHeadingSensorBatch& Batch);
	static bool PublishStepsBatch(const FOpenMobileStepsSensorBatch& Batch);
	static bool PublishActivityBatch(
		const FOpenMobileActivitySensorBatch& Batch
	);
	static bool PublishOrientationBatch(
		const FOpenMobileOrientationSensorBatch& Batch
	);
	static bool PublishProximityBatch(
		const FOpenMobileProximitySensorBatch& Batch
	);
	static bool PublishVectorBatchFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileVectorSensorBatch& Batch
	);
	static bool PublishAttitudeBatchFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileAttitudeSensorBatch& Batch
	);
	static bool PublishScalarBatchFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileScalarSensorBatch& Batch
	);
	static bool PublishHeadingBatchFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileHeadingSensorBatch& Batch
	);
	static bool PublishStepsBatchFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileStepsSensorBatch& Batch
	);
	static bool PublishActivityBatchFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileActivitySensorBatch& Batch
	);
	static bool PublishOrientationBatchFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileOrientationSensorBatch& Batch
	);
	static bool PublishProximityBatchFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileProximitySensorBatch& Batch
	);
	static bool PublishAccuracyFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
		const FOpenMobileSensorAccuracySnapshot& Snapshot
	);

	static bool ReadLatestVector(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileVectorSensorSample& OutSample
	);
	static bool ReadLatestAttitude(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileAttitudeSensorSample& OutSample
	);
	static bool ReadLatestScalar(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	);
	static bool ReadLatestHeading(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileHeadingSensorSample& OutSample
	);
	static bool ReadLatestSteps(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	);
	static bool ReadLatestActivity(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileActivitySensorSample& OutSample
	);
	static bool ReadLatestOrientation(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileOrientationSensorSample& OutSample
	);
	static bool ReadLatestProximity(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileProximitySensorSample& OutSample
	);
	static bool DrainBufferedVector(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileVectorSensorBatch& OutBatch
	);
	static bool DrainBufferedAttitude(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileAttitudeSensorBatch& OutBatch
	);
	static bool DrainBufferedScalar(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileScalarSensorBatch& OutBatch
	);
	static bool DrainBufferedHeading(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileHeadingSensorBatch& OutBatch
	);
	static bool DrainBufferedSteps(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileStepsSensorBatch& OutBatch
	);
	static bool DrainBufferedActivity(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileActivitySensorBatch& OutBatch
	);
	static bool DrainBufferedOrientation(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileOrientationSensorBatch& OutBatch
	);
	static bool DrainBufferedProximity(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileProximitySensorBatch& OutBatch
	);
	static FOnOpenMobileVectorSensorBatchReady& OnVectorBatch();
	static FOnOpenMobileAttitudeSensorBatchReady& OnAttitudeBatch();
	static FOnOpenMobileScalarSensorBatchReady& OnScalarBatch();
	static FOnOpenMobileHeadingSensorBatchReady& OnHeadingBatch();
	static FOnOpenMobileStepsSensorBatchReady& OnStepsBatch();
	static FOnOpenMobileActivitySensorBatchReady& OnActivityBatch();
	static FOnOpenMobileOrientationSensorBatchReady& OnOrientationBatch();
	static FOnOpenMobileProximitySensorBatchReady& OnProximityBatch();
	static FOnOpenMobileSensorAccuracyChangedReady& OnAccuracyChanged();
	static FOnOpenMobileSensorCalibrationChangedReady& OnCalibrationChanged();

#if WITH_DEV_AUTOMATION_TESTS
	static void DrainPendingEventsForTests(double NowSeconds);
	static void ResetForTests();
#endif
};

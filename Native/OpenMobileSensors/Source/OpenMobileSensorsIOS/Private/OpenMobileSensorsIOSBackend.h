#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"
#include "OpenMobileNativeStepCounter.h"

struct FOpenMobileSensorsBackendToken;
class FOpenMobileSensorsIOSBridge;
enum class EOpenMobileSensorsIOSBridgeFailure : uint8;
struct FOpenMobileSensorsIOSAvailability;

class FOpenMobileSensorsIOSBackend final : public IOpenMobileSensorsBackend
{
public:
	FOpenMobileSensorsIOSBackend();
	virtual ~FOpenMobileSensorsIOSBackend() override;
	virtual FName GetBackendName() const override;
	virtual FOpenMobileCapability GetBackendCapability() const override;
	virtual TArray<FOpenMobileSensorCapability> GetSensorCapabilities() const override;
	virtual TArray<FOpenMobileSensorBackendMetadata> GetSensorMetadata() const override;
	virtual FOpenMobileSensorOperationResult StartSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	) override;
	virtual FOpenMobileSensorOperationResult ReconfigureSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	) override;
	virtual void StopSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle
	) override;
	virtual FOpenMobileSensorOperationResult FlushSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FGuid& RequestId,
		FOnOpenMobileSensorBackendFlushComplete&& Completion
	) override;
	virtual FOpenMobileSensorOperationResult QueryNativeStepCount(
		const FGuid& RequestId,
		const FOpenMobileNativeStepCountQuery& Query,
		FOnOpenMobileNativeStepCountBackendQueryComplete&& Completion
	) override;
	virtual bool CancelNativeStepCountQuery(
		const FGuid& RequestId
	) override;
	virtual void BeginShutdown() override;
	static double ConvertCoreMotionTimestampSeconds(double TimestampSeconds);
	static bool CaptureApplicationWindowRotationFromMainThread(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorScreenRotation Rotation,
		double TimestampSeconds,
		bool bNaturalOrientationLandscape
	);

	bool PublishVectorBatchFromMotionQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileVectorSensorBatch& Batch
	);
	bool PublishAttitudeBatchFromMotionQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileAttitudeSensorBatch& Batch
	);
	bool PublishScalarBatchFromMotionQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileScalarSensorBatch& Batch
	);
	bool PublishHeadingBatchFromMotionQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileHeadingSensorBatch& Batch
	);
	bool PublishProximityBatchFromProximityQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileProximitySensorBatch& Batch
	);
	bool PublishStepsBatchFromPedometerQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileStepsSensorBatch& Batch
	);
	bool PublishActivityBatchFromMotionQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileActivitySensorBatch& Batch
	);
	bool PublishMagneticFieldAccuracyFromMotionQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorIdentifier& Sensor,
		int32 NativeAccuracy,
		double TimestampSeconds
	);
	bool PublishHeadingAccuracyFromLocationCallback(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorIdentifier& Sensor,
		double AccuracyDegrees,
		bool bCalibrationRequired,
		double TimestampSeconds
	);
	void FailPhysicalStreamFromBackend(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		EOpenMobileSensorsIOSBridgeFailure Failure,
		FString NativeDomain,
		FString NativeCode
	);
	FOpenMobileSensorOperationResult MapBridgeFailure(
		EOpenMobileSensorsIOSBridgeFailure Failure,
		FString NativeDomain = {},
		FString NativeCode = {}
	) const;

private:
	friend class FOpenMobileSensorsIOSBridge;

	FOpenMobileSensorsIOSBridge& GetBridge() const;
	FOpenMobileSensorsIOSAvailability QueryAvailability() const;

	mutable TUniquePtr<FOpenMobileSensorsIOSBridge> Bridge;
	FCriticalSection NativeStepCountersMutex;
	TMap<FGuid, FOpenMobileNativeStepCounterTracker> NativeStepCounters;
	mutable TAtomic<uint8> LastBridgeFailure = 0;
	TAtomic<bool> bShuttingDown = false;
};

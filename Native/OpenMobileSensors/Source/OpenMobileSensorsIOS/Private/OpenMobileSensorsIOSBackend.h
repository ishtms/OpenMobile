#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"

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

private:
	friend class FOpenMobileSensorsIOSBridge;

	FOpenMobileSensorsIOSBridge& GetBridge() const;
	FOpenMobileSensorsIOSAvailability QueryAvailability() const;
	FOpenMobileSensorOperationResult MapBridgeFailure(
		EOpenMobileSensorsIOSBridgeFailure Failure,
		FString NativeDomain = {},
		FString NativeCode = {}
	) const;

	mutable TUniquePtr<FOpenMobileSensorsIOSBridge> Bridge;
	mutable TAtomic<uint8> LastBridgeFailure = 0;
	TAtomic<bool> bShuttingDown = false;
};

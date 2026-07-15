#pragma once

#include "CoreMinimal.h"
#include "IOpenMobilePermissionProvider.h"
#include "IOpenMobileMotionActivityProvider.h"
#include "IOpenMobileSensorsBackend.h"
#include "OpenMobileNativeStepCounter.h"

class FOpenMobileSensorsAndroidBridge;
enum class EOpenMobileSensorsAndroidBridgeFailure : uint8;
struct FOpenMobileSensorsAndroidSensorDescriptor;
struct FOpenMobileSensorsBackendToken;
class IModularFeature;

class FOpenMobileSensorsAndroidBackend final
	: public IOpenMobileSensorsBackend
	, public IOpenMobilePermissionProvider
{
public:
	FOpenMobileSensorsAndroidBackend();
	virtual ~FOpenMobileSensorsAndroidBackend() override;
	virtual FName GetBackendName() const override;
	virtual FName GetProviderName() const override;
	virtual bool SupportsPermission(FName Permission) const override;
	virtual FOpenMobilePermissionResult GetStatus(
		FName Permission
	) const override;
	virtual bool RequestPermission(
		FName Permission,
		const FGuid& RequestIdentifier,
		FOpenMobileNativePermissionCompletion&& Completion,
		FOpenMobileError& OutError
	) override;
	virtual void CancelRequest(const FGuid& RequestIdentifier) override;
	virtual FOpenMobileCapability GetBackendCapability() const override;
	virtual TArray<FOpenMobileSensorCapability> GetSensorCapabilities() const override;
	virtual TArray<FOpenMobileSensorBackendMetadata> GetSensorMetadata() const override;
	virtual bool RequiresHighSamplingRateDeclaration() const override;
	virtual bool HasHighSamplingRateDeclaration() const override;
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
	static double ConvertSensorEventTimestampNanoseconds(
		int64 TimestampNanoseconds
	);
	static bool CaptureApplicationWindowRotationFromUIThread(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorScreenRotation Rotation,
		double TimestampSeconds,
		bool bNaturalOrientationLandscape
	);

	bool PublishVectorBatchFromHandler(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileVectorSensorBatch& Batch
	);
	bool PublishAccuracyFromHandler(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorIdentifier& Sensor,
		int32 NativeAccuracy,
		double TimestampSeconds
	);
	bool PublishCompactBatchFromHandler(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor,
		int32 SampleCount,
		int32 ValuesPerSample,
		int32 ValueStride,
		TArray<int64>&& TimestampsNanoseconds,
		TArray<float>&& Values,
		bool bResetFirstSample,
		EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame
	);
	void HandlePhysicalStreamFailureFromHandler(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		EOpenMobileSensorsAndroidBridgeFailure Failure,
		FString NativeCode
	);
	void HandleSensorsChangedFromHandler();

private:
	friend class FOpenMobileSensorsAndroidBridge;

	FOpenMobileSensorsAndroidBridge& GetBridge() const;
	FOpenMobileSensorOperationResult MapBridgeFailure(
		EOpenMobileSensorsAndroidBridgeFailure Failure,
		FString NativeCode = {}
	) const;
	bool QuerySensorDescriptors(
		TArray<FOpenMobileSensorsAndroidSensorDescriptor>& OutDescriptors,
		FOpenMobileSensorOperationResult* OutFailure = nullptr
	) const;
	bool SelectDescriptor(
		const FOpenMobileSensorPhysicalStreamRequest& Request,
		const TArray<FOpenMobileSensorsAndroidSensorDescriptor>& Descriptors,
		FOpenMobileSensorsAndroidSensorDescriptor& OutDescriptor,
		FOpenMobileSensorOperationResult& OutFailure
	) const;
	void ResolveNativeRequest(
		const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest,
		int32& OutSamplingPeriodMicroseconds,
		int32& OutMaximumReportLatencyMicroseconds
	) const;
	FOpenMobileSensorOperationResult StartMotionActivityProviderStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	);
	FOpenMobileSensorOperationResult ReconfigureMotionActivityProviderStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	);
	bool StopMotionActivityProviderStream(
		const FOpenMobileSensorBackendStreamHandle& Handle
	);
	void HandleMotionActivityProviderUnregistered(
		const FName& FeatureName,
		IModularFeature* Feature
	);
	void HandleMotionActivityProviderRegistered(
		const FName& FeatureName,
		IModularFeature* Feature
	);

	struct FActiveMotionActivityProviderStream
	{
		IOpenMobileMotionActivityProvider* Provider = nullptr;
		FOpenMobileMotionActivityProviderStreamHandle ProviderHandle;
		FOpenMobileSensorBackendStreamHandle BackendHandle;
	};

	mutable TUniquePtr<FOpenMobileSensorsAndroidBridge> Bridge;
	FCriticalSection NativeStepCountersMutex;
	TMap<FGuid, FOpenMobileNativeStepCounterTracker> NativeStepCounters;
	FCriticalSection MotionActivityProviderStreamsMutex;
	TMap<FGuid, FActiveMotionActivityProviderStream>
		MotionActivityProviderStreams;
	FDelegateHandle MotionActivityProviderUnregisteredHandle;
	FDelegateHandle MotionActivityProviderRegisteredHandle;
	mutable TAtomic<uint8> LastBridgeFailure = 0;
	TAtomic<bool> bShuttingDown = false;
};

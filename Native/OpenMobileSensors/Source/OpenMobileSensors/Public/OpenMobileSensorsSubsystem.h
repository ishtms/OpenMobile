#pragma once

#include "CoreMinimal.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorDiagnostics.h"
#include "OpenMobileSensorMetadata.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileSensorsSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorCapabilitiesChanged,
	const FOpenMobileSensorCapabilitySnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorSubscriptionStateChanged,
	const FOpenMobileSensorSubscriptionStateSnapshot&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileVectorSensorBatch,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileVectorSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileAttitudeSensorBatch,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileAttitudeSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileScalarSensorBatch,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileScalarSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileHeadingSensorBatch,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileHeadingSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileStepsSensorBatch,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileStepsSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileActivitySensorBatch,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileActivitySensorBatch&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileOrientationSensorBatch,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileOrientationSensorBatch&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileProximitySensorBatch,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileProximitySensorBatch&
);
DECLARE_DELEGATE_OneParam(
	FOnOpenMobileSensorFlushComplete,
	const FOpenMobileSensorFlushResult&
);
DECLARE_DELEGATE_OneParam(
	FOnOpenMobileSensorRecenterComplete,
	const FOpenMobileSensorRecenterResult&
);

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorsSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	FOpenMobileSensorCapabilitySnapshot GetCapabilitySnapshotNative() const;
	TArray<FOpenMobileSensorMetadata> GetMetadataNative() const;

	FOpenMobileSensorSubscriptionResult StartSubscriptionNative(
		const FOpenMobileSensorSubscriptionRequest& Request
	);
	FOpenMobileSensorOperationResult UpdateSubscriptionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorStreamOptions& Options
	);
	FOpenMobileSensorOperationResult StopSubscriptionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	int32 StopAllSubscriptionsNative();
	bool GetSubscriptionStateNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOpenMobileSensorSubscriptionStateSnapshot& OutState
	) const;

	bool GetLatestVectorSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileVectorSensorSample& OutSample
	) const;
	bool GetLatestAttitudeSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileAttitudeSensorSample& OutSample
	) const;
	bool GetLatestScalarSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	) const;
	bool GetLatestHeadingSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileHeadingSensorSample& OutSample
	) const;
	bool GetLatestStepsSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	) const;
	bool GetLatestActivitySampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileActivitySensorSample& OutSample
	) const;
	bool GetLatestOrientationSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileOrientationSensorSample& OutSample
	) const;
	bool GetLatestProximitySampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileProximitySensorSample& OutSample
	) const;

	bool GetBufferedVectorSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileVectorSensorBatch& OutBatch
	);
	bool GetBufferedAttitudeSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileAttitudeSensorBatch& OutBatch
	);
	bool GetBufferedScalarSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileScalarSensorBatch& OutBatch
	);
	bool GetBufferedHeadingSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileHeadingSensorBatch& OutBatch
	);
	bool GetBufferedStepsSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileStepsSensorBatch& OutBatch
	);
	bool GetBufferedActivitySamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileActivitySensorBatch& OutBatch
	);
	bool GetBufferedOrientationSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileOrientationSensorBatch& OutBatch
	);
	bool GetBufferedProximitySamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileProximitySensorBatch& OutBatch
	);

	FGuid FlushNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOnOpenMobileSensorFlushComplete&& Completion
	);
	FGuid RecenterNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode,
		FOnOpenMobileSensorRecenterComplete&& Completion
	);

	FOpenMobilePermissionResult GetPermissionStatusNative(
		EOpenMobileSensorPermission Permission
	) const;
	FOpenMobilePermissionRequestHandle RequestPermissionNative(
		EOpenMobileSensorPermission Permission,
		FOnOpenMobilePermissionRequestComplete&& Completion
	);
	bool CancelPermissionRequestNative(
		const FOpenMobilePermissionRequestHandle& Handle
	);
	FOpenMobileSensorOperationResult SetTrueHeadingLocationInputNative(
		const FOpenMobileSensorLocationInput& LocationInput
	);

	FOpenMobileSensorDiagnosticsSnapshot GetDiagnosticsSnapshotNative() const;

	FOnOpenMobileSensorCapabilitiesChanged& OnCapabilitiesChangedNative();
	FOnOpenMobileSensorSubscriptionStateChanged& OnSubscriptionStateChangedNative();
	FOnOpenMobileVectorSensorBatch& OnVectorSamplesNative();
	FOnOpenMobileAttitudeSensorBatch& OnAttitudeSamplesNative();
	FOnOpenMobileScalarSensorBatch& OnScalarSamplesNative();
	FOnOpenMobileHeadingSensorBatch& OnHeadingSamplesNative();
	FOnOpenMobileStepsSensorBatch& OnStepsSamplesNative();
	FOnOpenMobileActivitySensorBatch& OnActivitySamplesNative();
	FOnOpenMobileOrientationSensorBatch& OnOrientationSamplesNative();
	FOnOpenMobileProximitySensorBatch& OnProximitySamplesNative();

private:
	FOnOpenMobileSensorCapabilitiesChanged CapabilitiesChangedEvent;
	FOnOpenMobileSensorSubscriptionStateChanged SubscriptionStateChangedEvent;
	FOnOpenMobileVectorSensorBatch VectorSamplesEvent;
	FOnOpenMobileAttitudeSensorBatch AttitudeSamplesEvent;
	FOnOpenMobileScalarSensorBatch ScalarSamplesEvent;
	FOnOpenMobileHeadingSensorBatch HeadingSamplesEvent;
	FOnOpenMobileStepsSensorBatch StepsSamplesEvent;
	FOnOpenMobileActivitySensorBatch ActivitySamplesEvent;
	FOnOpenMobileOrientationSensorBatch OrientationSamplesEvent;
	FOnOpenMobileProximitySensorBatch ProximitySamplesEvent;
};

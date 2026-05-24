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

class UOpenMobileSensorAsyncActionBase;

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
DECLARE_DELEGATE_OneParam(
	FOnOpenMobileSensorRecordingComplete,
	const FOpenMobileSensorRecordingResult&
);
DECLARE_DELEGATE_OneParam(
	FOnOpenMobileSensorReplayComplete,
	const FOpenMobileSensorReplayResult&
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileSensorCapabilitiesChangedDynamic,
	FOpenMobileSensorCapabilitySnapshot,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileSensorSubscriptionStateChangedDynamic,
	FOpenMobileSensorSubscriptionStateSnapshot,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileVectorSensorBatchDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileVectorSensorBatch,
	Batch
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileAttitudeSensorBatchDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileAttitudeSensorBatch,
	Batch
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileScalarSensorBatchDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileScalarSensorBatch,
	Batch
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHeadingSensorBatchDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileHeadingSensorBatch,
	Batch
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileStepsSensorBatchDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileStepsSensorBatch,
	Batch
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileActivitySensorBatchDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileActivitySensorBatch,
	Batch
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileOrientationSensorBatchDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileOrientationSensorBatch,
	Batch
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileProximitySensorBatchDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileProximitySensorBatch,
	Batch
);

UCLASS(BlueprintType)
class OPENMOBILESENSORS_API UOpenMobileSensorsSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Sensor Capabilities", ToolTip = "Returns a side-effect-free snapshot of sensor availability and restrictions."))
	FOpenMobileSensorCapabilitySnapshot GetCapabilitySnapshotNative() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Sensor Metadata", ToolTip = "Returns cached portable metadata for discovered sensors without starting hardware."))
	TArray<FOpenMobileSensorMetadata> GetMetadataNative() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Start Sensor Subscription", ToolTip = "Accepts a sensor stream request and returns a typed handle or failure."))
	FOpenMobileSensorSubscriptionResult StartSubscriptionNative(
		const FOpenMobileSensorSubscriptionRequest& Request
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Update Sensor Subscription", ToolTip = "Updates an active subscription without replacing its handle when supported."))
	FOpenMobileSensorOperationResult UpdateSubscriptionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorStreamOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Stop Sensor Subscription", ToolTip = "Stops one owned subscription and returns an explicit invalid-handle result when needed."))
	FOpenMobileSensorOperationResult StopSubscriptionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Stop All Sensor Subscriptions", ToolTip = "Stops every subscription owned by this Game Instance and returns the number stopped."))
	int32 StopAllSubscriptionsNative();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Sensor Subscription State", ToolTip = "Returns the current state for an owned subscription handle."))
	bool GetSubscriptionStateNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOpenMobileSensorSubscriptionStateSnapshot& OutState
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Latest Vector Sample", ToolTip = "Copies the latest vector sample and reports age, validity, and sequence state."))
	bool GetLatestVectorSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileVectorSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Latest Attitude Sample", ToolTip = "Copies the latest attitude sample and reports age, validity, and sequence state."))
	bool GetLatestAttitudeSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileAttitudeSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Latest Scalar Sample", ToolTip = "Copies the latest scalar sample and reports age, validity, and sequence state."))
	bool GetLatestScalarSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Latest Heading Sample", ToolTip = "Copies the latest heading sample and reports age, validity, and sequence state."))
	bool GetLatestHeadingSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileHeadingSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Latest Steps Sample", ToolTip = "Copies the latest steps sample and reports age, validity, and sequence state."))
	bool GetLatestStepsSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Latest Activity Sample", ToolTip = "Copies the latest activity sample and reports age, validity, and sequence state."))
	bool GetLatestActivitySampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileActivitySensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Latest Orientation Sample", ToolTip = "Copies the latest physical-orientation sample and reports age and sequence state."))
	bool GetLatestOrientationSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileOrientationSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Latest Proximity Sample", ToolTip = "Copies the latest proximity sample and reports age, validity, and sequence state."))
	bool GetLatestProximitySampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileProximitySensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Buffered Vector Samples", ToolTip = "Drains a bounded number of ordered vector samples and returns overflow counters."))
	bool GetBufferedVectorSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileVectorSensorBatch& OutBatch
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Buffered Attitude Samples", ToolTip = "Drains a bounded number of ordered attitude samples and returns overflow counters."))
	bool GetBufferedAttitudeSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileAttitudeSensorBatch& OutBatch
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Buffered Scalar Samples", ToolTip = "Drains a bounded number of ordered scalar samples and returns overflow counters."))
	bool GetBufferedScalarSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileScalarSensorBatch& OutBatch
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Buffered Heading Samples", ToolTip = "Drains a bounded number of ordered heading samples and returns overflow counters."))
	bool GetBufferedHeadingSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileHeadingSensorBatch& OutBatch
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Buffered Steps Samples", ToolTip = "Drains a bounded number of ordered steps samples and returns overflow counters."))
	bool GetBufferedStepsSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileStepsSensorBatch& OutBatch
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Buffered Activity Samples", ToolTip = "Drains a bounded number of ordered activity samples and returns overflow counters."))
	bool GetBufferedActivitySamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileActivitySensorBatch& OutBatch
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Buffered Orientation Samples", ToolTip = "Drains a bounded number of ordered orientation samples and returns overflow counters."))
	bool GetBufferedOrientationSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileOrientationSensorBatch& OutBatch
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Buffered Proximity Samples", ToolTip = "Drains a bounded number of ordered proximity samples and returns overflow counters."))
	bool GetBufferedProximitySamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Recenter Sensor Attitude", ToolTip = "Recenters an attitude subscription or returns an explicit unsupported or invalid-handle result."))
	FOpenMobileSensorRecenterResult RecenterSubscription(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Sensor Permission Status", ToolTip = "Returns normalized permission state without displaying a system prompt."))
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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Set True Heading Location Input", ToolTip = "Supplies narrow caller-owned location input without starting location services."))
	FOpenMobileSensorOperationResult SetTrueHeadingLocationInputNative(
		const FOpenMobileSensorLocationInput& LocationInput
	);
	FGuid StartRecordingNative(
		const FOpenMobileSensorRecordingOptions& Options,
		FOnOpenMobileSensorRecordingComplete&& Completion
	);
	FGuid StopRecordingNative(
		FGuid RequestId,
		FOnOpenMobileSensorRecordingComplete&& Completion
	);
	FGuid ReplayRecordingNative(
		const FString& FilePath,
		const FOpenMobileSensorReplayOptions& Options,
		FOnOpenMobileSensorReplayComplete&& Completion
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Sensor Diagnostics", ToolTip = "Returns a read-only diagnostics snapshot without changing sensor state."))
	FOpenMobileSensorDiagnosticsSnapshot GetDiagnosticsSnapshotNative() const;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Sensor Capabilities Changed", ToolTip = "Broadcast when a material sensor capability field changes."))
	FOpenMobileSensorCapabilitiesChangedDynamic OnCapabilitiesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Sensor Subscription State Changed", ToolTip = "Broadcast when an owned subscription changes state."))
	FOpenMobileSensorSubscriptionStateChangedDynamic OnSubscriptionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Vector Sensor Samples", ToolTip = "Broadcast a rate-capped batch of vector samples on the game thread."))
	FOpenMobileVectorSensorBatchDynamic OnVectorSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Attitude Sensor Samples", ToolTip = "Broadcast a rate-capped batch of attitude samples on the game thread."))
	FOpenMobileAttitudeSensorBatchDynamic OnAttitudeSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Scalar Sensor Samples", ToolTip = "Broadcast a rate-capped batch of scalar samples on the game thread."))
	FOpenMobileScalarSensorBatchDynamic OnScalarSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Heading Sensor Samples", ToolTip = "Broadcast a rate-capped batch of heading samples on the game thread."))
	FOpenMobileHeadingSensorBatchDynamic OnHeadingSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Steps Sensor Samples", ToolTip = "Broadcast a rate-capped batch of steps samples on the game thread."))
	FOpenMobileStepsSensorBatchDynamic OnStepsSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Activity Sensor Samples", ToolTip = "Broadcast a rate-capped batch of activity samples on the game thread."))
	FOpenMobileActivitySensorBatchDynamic OnActivitySamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Orientation Sensor Samples", ToolTip = "Broadcast a rate-capped batch of physical-orientation samples on the game thread."))
	FOpenMobileOrientationSensorBatchDynamic OnOrientationSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Proximity Sensor Samples", ToolTip = "Broadcast a rate-capped batch of proximity samples on the game thread."))
	FOpenMobileProximitySensorBatchDynamic OnProximitySamples;

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
	friend class UOpenMobileSensorAsyncActionBase;

	void RegisterAsyncAction(UOpenMobileSensorAsyncActionBase* Action);
	void UnregisterAsyncAction(UOpenMobileSensorAsyncActionBase* Action);

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
	TSet<TWeakObjectPtr<UOpenMobileSensorAsyncActionBase>> AsyncActions;
};

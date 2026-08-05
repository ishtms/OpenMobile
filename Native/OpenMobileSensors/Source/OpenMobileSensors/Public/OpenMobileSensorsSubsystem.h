#pragma once

#include "CoreMinimal.h"
#include "OpenMobileNativeStepCount.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorCalibration.h"
#include "OpenMobileSensorDiagnostics.h"
#include "OpenMobileSensorErrorReport.h"
#include "OpenMobileSensorMetadata.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileStepCountSession.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileSensorsSubsystem.generated.h"

class UOpenMobileSensorAsyncActionBase;
class UOpenMobileSensorListener;
class UOpenMobileSensorRecordingSession;
class UOpenMobileSensorReplaySession;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorCapabilitiesChanged,
	const FOpenMobileSensorCapabilitySnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorSubscriptionStateChanged,
	const FOpenMobileSensorSubscriptionStateSnapshot&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileSensorAccuracyChanged,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileSensorAccuracySnapshot&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileSensorCalibrationChanged,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileSensorCalibrationEvent&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileSensorSamplesDropped,
	FOpenMobileSensorSubscriptionHandle,
	const FOpenMobileSensorDropInfo&
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnOpenMobileSensorError,
	const FOpenMobileSensorSubscriptionHandle&,
	const FOpenMobileSensorRuntimeError&
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
DECLARE_DELEGATE_OneParam(
	FOnOpenMobileNativeStepCountQueryComplete,
	const FOpenMobileNativeStepCountQueryResult&
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
	FOpenMobileSensorAccuracyChangedDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileSensorAccuracySnapshot,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileSensorCalibrationChangedDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileSensorCalibrationEvent,
	Event
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileSensorSamplesDroppedDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileSensorDropInfo,
	DropInfo
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileSensorErrorDynamic,
	FOpenMobileSensorSubscriptionHandle,
	Handle,
	FOpenMobileSensorRuntimeError,
	Error
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

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Coordinates", meta = (DisplayName = "Notify Current Screen Rotation", Keywords = "OpenMobile sensors screen window orientation rotation override", ToolTip = "Overrides the automatically tracked application-window rotation. The plugin supplies its monotonic timestamp and discovered natural orientation."))
	bool NotifyCurrentScreenRotationNative(
		EOpenMobileSensorScreenRotation Rotation
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Coordinates", meta = (DisplayName = "Update Application Window Rotation (Timestamped)", Keywords = "OpenMobile sensors screen window orientation rotation monotonic advanced", ToolTip = "Advanced override using FPlatformTime::Seconds monotonic time from this process and an explicitly known natural device orientation."))
	bool UpdateApplicationWindowRotationNative(
		EOpenMobileSensorScreenRotation Rotation,
		double TimestampSeconds,
		bool bNaturalOrientationLandscape
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Replace Sensor Subscription Options (Advanced)", ToolTip = "Advanced raw-handle path that replaces the complete options struct. Prefer focused setters on a typed sensor listener."))
	FOpenMobileSensorOperationResult UpdateSubscriptionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorStreamOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Stop Sensor Subscription", ToolTip = "Stops one owned subscription and returns an explicit invalid-handle result when needed."))
	FOpenMobileSensorOperationResult StopSubscriptionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Stop All Game Instance Sensor Subscriptions (Advanced)", Keywords = "OpenMobile sensors raw handles Game Instance stop all", ToolTip = "Advanced raw-handle cleanup. Stops every sensor subscription owned by this Game Instance, but does not stop typed recording or replay sessions. Prefer Stop Sensor Listeners for scoped typed cleanup."))
	int32 StopAllSubscriptionsNative();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Listeners", meta = (DisplayName = "Get Managed Sensor Listeners", Keywords = "OpenMobile sensors listeners active paused starting Game Instance collection", ToolTip = "Returns unfinished typed listeners owned by this Game Instance, including starting, active, and paused listeners."))
	TArray<UOpenMobileSensorListener*> GetManagedSensorListenersNative() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Sensor Subscription State", ToolTip = "Returns the current state, including attitude reference and recenter state, for an owned subscription handle."))
	bool GetSubscriptionStateNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOpenMobileSensorSubscriptionStateSnapshot& OutState
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Vector Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached vector snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestVectorSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileVectorSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Attitude Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached attitude snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestAttitudeSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileAttitudeSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Scalar Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached scalar snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestScalarSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Heading Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached heading snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestHeadingSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileHeadingSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Steps Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached steps snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestStepsSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Activity Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached activity snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestActivitySampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileActivitySensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Orientation Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached orientation snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestOrientationSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileOrientationSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Proximity Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached proximity snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
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
	bool CancelFlushNative(FGuid RequestId);
	FGuid RecenterNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode,
		FOnOpenMobileSensorRecenterComplete&& Completion
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Recenter Sensor Attitude by Handle (Advanced)", ToolTip = "Advanced raw-handle path. Prefer Recenter Attitude or Clear Attitude Recenter on a typed attitude listener."))
	FOpenMobileSensorRecenterResult RecenterSubscription(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Begin Relative Altitude Session by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Listen for Relative Altitude and keep the typed listener object.", ToolTip = "Legacy raw-handle session. Prefer the typed relative-altitude listener."))
	FOpenMobileSensorSubscriptionResult BeginRelativeAltitudeSessionNative(
		const FOpenMobileSensorStreamOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Recenter Relative Altitude by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Recenter Relative Altitude on a typed relative-altitude listener.", ToolTip = "Legacy raw-handle recenter. Prefer the typed listener control."))
	FOpenMobileSensorOperationResult RecenterRelativeAltitudeBaselineNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Read Relative Altitude by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Get Latest Relative Altitude on a typed relative-altitude listener.", ToolTip = "Legacy raw-handle read. Prefer the typed listener snapshot."))
	bool ReadRelativeAltitudeSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Stop Relative Altitude by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Stop Sensor Listener on the typed relative-altitude listener.", ToolTip = "Legacy raw-handle stop. Prefer stopping the typed listener."))
	FOpenMobileSensorOperationResult StopRelativeAltitudeSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Begin Step Count Session by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Listen for Step Count and keep the typed listener object.", ToolTip = "Legacy raw-handle session. Prefer the typed step-count listener."))
	FOpenMobileSensorSubscriptionResult BeginStepCountSessionNative(
		const FOpenMobileSensorStreamOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Reset Step Count by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Reset Step Count on a typed step-count listener.", ToolTip = "Legacy raw-handle reset. Prefer the typed listener control."))
	FOpenMobileSensorOperationResult ResetStepCountSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Read Step Count by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Get Latest Step Count on a typed step-count listener.", ToolTip = "Legacy raw-handle read. Prefer the typed listener snapshot."))
	bool ReadStepCountSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Stop Step Count by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Stop Sensor Listener on the typed step-count listener.", ToolTip = "Legacy raw-handle stop. Prefer stopping the typed listener."))
	FOpenMobileSensorOperationResult StopStepCountSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Step Count Session Policy", ToolTip = "Returns the lifetime and discontinuity policy used by resettable step sessions."))
	FOpenMobileStepCountSessionPolicy GetStepCountSessionPolicyNative() const;
	FGuid QueryNativeStepCountNative(
		const FOpenMobileNativeStepCountQuery& Query,
		FOnOpenMobileNativeStepCountQueryComplete&& Completion
	);
	bool CancelNativeStepCountQueryNative(const FGuid& RequestId);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Request Calibration by Handle (Advanced)", ToolTip = "Advanced raw-handle path. Prefer the calibration node exposed by a typed attitude, magnetometer, or heading listener."))
	FOpenMobileSensorOperationResult RequestNativeCalibrationPrompt(
		const FOpenMobileSensorSubscriptionHandle& Handle
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

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Set True Heading Location", ExpandEnumAsExecs = "Outcome", Keywords = "OpenMobile sensors true north location GPS authorization", AdvancedDisplay = "AltitudeMeters,Details", ToolTip = "Supplies a fresh authorized fix from an external location provider. This plugin does not start location services or request location permission."))
	void SetTrueHeadingLocation(
		double LatitudeDegrees,
		double LongitudeDegrees,
		double HorizontalAccuracyMeters,
		FDateTime CapturedAtUtc,
		double AltitudeMeters,
		EOpenMobileTrueHeadingLocationOutcome& Outcome,
		FString& Message,
		FString& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Pose and Heading", meta = (DisplayName = "Set True Heading Location Input (Advanced)", ToolTip = "Advanced raw-struct path using a Unix timestamp. Prefer Set True Heading Location with a UTC date-time."))
	FOpenMobileSensorOperationResult SetTrueHeadingLocationInputNative(
		const FOpenMobileSensorLocationInput& LocationInput
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Clear True Heading Location", ToolTip = "Immediately removes caller-owned location retained for true heading."))
	FOpenMobileSensorOperationResult ClearTrueHeadingLocationInputNative();
	FGuid StartRecordingNative(
		const FOpenMobileSensorRecordingOptions& Options,
		FOnOpenMobileSensorRecordingComplete&& Completion
	);
	FGuid StopRecordingNative(
		FGuid RequestId,
		FOnOpenMobileSensorRecordingComplete&& Completion
	);
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (DisplayName = "Cancel Sensor Recording by GUID (Advanced)", ToolTip = "Cancels and discards one raw recording request. Prefer Discard Sensor Recording on a typed session."))
	FOpenMobileSensorOperationResult CancelRecordingNative(FGuid RequestId);
	void ReleaseRecordingSessionNative(FGuid RequestId);
	TArray<FOpenMobileSensorIdentifier>
	GetActiveSensorIdentifiersNative() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Active Sensor Recordings", Keywords = "OpenMobile sensors recording session active list", ToolTip = "Returns owner-scoped typed recording sessions that are starting, recording, or finalizing."))
	TArray<UOpenMobileSensorRecordingSession*>
	GetActiveRecordingSessionsNative() const;
	FGuid ReplayRecordingNative(
		const FString& FilePath,
		const FOpenMobileSensorReplayOptions& Options,
		FOnOpenMobileSensorReplayComplete&& Completion
	);
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Cancel Sensor Replay by GUID (Advanced)", ToolTip = "Cancels one raw replay request. Prefer Stop Sensor Replay on a typed session."))
	FOpenMobileSensorOperationResult CancelReplayNative(FGuid RequestId);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Active Sensor Replays", Keywords = "OpenMobile sensors replay session active list", ToolTip = "Returns owner-scoped typed replay sessions that are loading, playing, or paused."))
	TArray<UOpenMobileSensorReplaySession*>
	GetActiveReplaySessionsNative() const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Pause Sensor Replay by GUID (Advanced)", ToolTip = "Pauses one raw replay request without losing its playback position."))
	FOpenMobileSensorOperationResult PauseReplayNative(FGuid RequestId);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Resume Sensor Replay by GUID (Advanced)", ToolTip = "Resumes one paused raw replay request."))
	FOpenMobileSensorOperationResult ResumeReplayNative(FGuid RequestId);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Seek Sensor Replay by GUID (Advanced)", ToolTip = "Moves one raw replay request to a validated recording-relative time in seconds."))
	FOpenMobileSensorOperationResult SeekReplayNative(
		FGuid RequestId,
		double PlaybackTimeSeconds
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Set Sensor Replay Speed by GUID (Advanced)", ToolTip = "Changes raw replay speed while preserving the current playback position."))
	FOpenMobileSensorOperationResult SetReplaySpeedNative(
		FGuid RequestId,
		double PlaybackSpeed
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Set Sensor Replay Looping by GUID (Advanced)", ToolTip = "Enables or disables looping for one raw replay request."))
	FOpenMobileSensorOperationResult SetReplayLoopingNative(
		FGuid RequestId,
		bool bLoop
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Advance Manual Sensor Replay by GUID (Advanced)", ToolTip = "Advances a raw manual-clock replay by a finite nonnegative duration in seconds."))
	FOpenMobileSensorOperationResult AdvanceReplayNative(
		FGuid RequestId,
		double DeltaSeconds
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Get Sensor Replay State by GUID (Advanced)", ToolTip = "Returns one raw replay request state without advancing it."))
	bool GetReplayStateNative(
		FGuid RequestId,
		FOpenMobileSensorReplaySnapshot& OutSnapshot
	) const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Sensor Diagnostics", ToolTip = "Returns a read-only diagnostics snapshot without changing sensor state."))
	FOpenMobileSensorDiagnosticsSnapshot GetDiagnosticsSnapshotNative() const;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Sensor Capabilities Changed", ToolTip = "Broadcast when a material sensor capability field changes."))
	FOpenMobileSensorCapabilitiesChangedDynamic OnCapabilitiesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Sensor Subscription State Changed", ToolTip = "Broadcast when an owned subscription changes state."))
	FOpenMobileSensorSubscriptionStateChangedDynamic OnSubscriptionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Sensor Accuracy Changed", ToolTip = "Broadcast the initial accuracy state and later quality or calibration changes on the game thread."))
	FOpenMobileSensorAccuracyChangedDynamic OnAccuracyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Sensor Calibration Changed", ToolTip = "Broadcast deduplicated calibration-required and resolution guidance on the game thread."))
	FOpenMobileSensorCalibrationChangedDynamic OnCalibrationChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "On Sensor Samples Dropped", ToolTip = "Broadcast one coalesced loss report per game-thread dispatch cycle for an affected subscription."))
	FOpenMobileSensorSamplesDroppedDynamic OnSamplesDropped;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Sensor Error", ToolTip = "Broadcast a compact runtime error for an owned sensor stream without requiring diagnostics polling."))
	FOpenMobileSensorErrorDynamic OnSensorError;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Vector Sensor Samples", ToolTip = "Broadcast a rate-capped batch of vector samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileVectorSensorBatchDynamic OnVectorSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Attitude Sensor Samples", ToolTip = "Broadcast a rate-capped batch of attitude samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileAttitudeSensorBatchDynamic OnAttitudeSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Scalar Sensor Samples", ToolTip = "Broadcast a rate-capped batch of scalar samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileScalarSensorBatchDynamic OnScalarSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Heading Sensor Samples", ToolTip = "Broadcast a rate-capped batch of heading samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileHeadingSensorBatchDynamic OnHeadingSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Steps Sensor Samples", ToolTip = "Broadcast a rate-capped batch of steps samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileStepsSensorBatchDynamic OnStepsSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Activity Sensor Samples", ToolTip = "Broadcast a rate-capped batch of activity samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileActivitySensorBatchDynamic OnActivitySamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Orientation Sensor Samples", ToolTip = "Broadcast a rate-capped batch of physical-orientation samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileOrientationSensorBatchDynamic OnOrientationSamples;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Sensors", meta = (DisplayName = "On Proximity Sensor Samples", ToolTip = "Broadcast a rate-capped batch of proximity samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileProximitySensorBatchDynamic OnProximitySamples;

	FOnOpenMobileSensorCapabilitiesChanged& OnCapabilitiesChangedNative();
	FOnOpenMobileSensorSubscriptionStateChanged& OnSubscriptionStateChangedNative();
	FOnOpenMobileSensorAccuracyChanged& OnAccuracyChangedNative();
	FOnOpenMobileSensorCalibrationChanged& OnCalibrationChangedNative();
	FOnOpenMobileSensorSamplesDropped& OnSamplesDroppedNative();
	FOnOpenMobileSensorError& OnSensorErrorNative();
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
#if WITH_DEV_AUTOMATION_TESTS
	friend class FOpenMobileSensorsPermissionOwnerTeardownTest;
#endif

	FGuid GetOrCreateSubscriptionOwnerIdentifier();
	void EnsureCapabilityListener() const;
	void EnsureSubscriptionListener() const;
	void EnsureSampleListeners() const;
	void HandleCapabilitySnapshotChanged(
		const FOpenMobileSensorCapabilitySnapshot& Snapshot
	);
	void HandleSubscriptionStateChanged(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionStateSnapshot& Snapshot
	);
	void HandleAccuracyChanged(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorAccuracySnapshot& Snapshot
	);
	void HandleCalibrationChanged(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorCalibrationEvent& Event
	);
	void HandleSamplesDropped(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorDropInfo& DropInfo
	);
	void HandleVectorBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileVectorSensorBatch& Batch
	);
	void HandleAttitudeBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileAttitudeSensorBatch& Batch
	);
	void HandleScalarBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileScalarSensorBatch& Batch
	);
	void HandleHeadingBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileHeadingSensorBatch& Batch
	);
	void HandleStepsBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileStepsSensorBatch& Batch
	);
	void HandleActivityBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileActivitySensorBatch& Batch
	);
	void HandleOrientationBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileOrientationSensorBatch& Batch
	);
	void HandleProximityBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileProximitySensorBatch& Batch
	);
	void RegisterAsyncAction(UOpenMobileSensorAsyncActionBase* Action);
	void UnregisterAsyncAction(UOpenMobileSensorAsyncActionBase* Action);

	FOnOpenMobileSensorCapabilitiesChanged CapabilitiesChangedEvent;
	FOnOpenMobileSensorSubscriptionStateChanged SubscriptionStateChangedEvent;
	FOnOpenMobileSensorAccuracyChanged AccuracyChangedEvent;
	FOnOpenMobileSensorCalibrationChanged CalibrationChangedEvent;
	FOnOpenMobileSensorSamplesDropped SamplesDroppedEvent;
	FOnOpenMobileSensorError SensorErrorEvent;
	FOnOpenMobileVectorSensorBatch VectorSamplesEvent;
	FOnOpenMobileAttitudeSensorBatch AttitudeSamplesEvent;
	FOnOpenMobileScalarSensorBatch ScalarSamplesEvent;
	FOnOpenMobileHeadingSensorBatch HeadingSamplesEvent;
	FOnOpenMobileStepsSensorBatch StepsSamplesEvent;
	FOnOpenMobileActivitySensorBatch ActivitySamplesEvent;
	FOnOpenMobileOrientationSensorBatch OrientationSamplesEvent;
	FOnOpenMobileProximitySensorBatch ProximitySamplesEvent;
	TSet<TWeakObjectPtr<UOpenMobileSensorAsyncActionBase>> AsyncActions;
	mutable FDelegateHandle CapabilityServiceChangedHandle;
	mutable FDelegateHandle SubscriptionServiceChangedHandle;
	mutable FDelegateHandle AccuracyChangedReadyHandle;
	mutable FDelegateHandle CalibrationChangedReadyHandle;
	mutable FDelegateHandle SamplesDroppedReadyHandle;
	mutable FDelegateHandle VectorBatchReadyHandle;
	mutable FDelegateHandle AttitudeBatchReadyHandle;
	mutable FDelegateHandle ScalarBatchReadyHandle;
	mutable FDelegateHandle HeadingBatchReadyHandle;
	mutable FDelegateHandle StepsBatchReadyHandle;
	mutable FDelegateHandle ActivityBatchReadyHandle;
	mutable FDelegateHandle OrientationBatchReadyHandle;
	mutable FDelegateHandle ProximityBatchReadyHandle;
	FGuid SubscriptionOwnerIdentifier;
	bool bDeinitialized = false;
};

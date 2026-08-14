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
	/** Unreal calls this once for each Game Instance. Services and provider callbacks stay scoped to that instance from here. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Unreal calls this before the Game Instance disappears. Pending actions, listeners, and native callbacks are released here. */
	virtual void Deinitialize() override;

	/** You'll get one coherent live snapshot of sensor availability and restrictions. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Sensor Capabilities", ToolTip = "Copies one coherent live snapshot of sensor availability and restrictions."))
	FOpenMobileSensorCapabilitySnapshot GetCapabilitySnapshotNative() const;

	/** You'll get the current cached portable metadata for discovered sensors. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Sensor Metadata", ToolTip = "Copies the current cached portable metadata for discovered sensors."))
	TArray<FOpenMobileSensorMetadata> GetMetadataNative() const;

	/** Use this to submit a raw stream request and get its typed handle. Accepted means startup continues asynchronously, no? */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Start Sensor Subscription", ToolTip = "Accepts a sensor stream request and returns a typed handle or failure."))
	FOpenMobileSensorSubscriptionResult StartSubscriptionNative(
		const FOpenMobileSensorSubscriptionRequest& Request
	);

	/** Use this only when automatic application-window rotation tracking can't see your display change. The plugin supplies monotonic time and natural orientation itself. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Coordinates", meta = (DisplayName = "Notify Current Screen Rotation", Keywords = "OpenMobile sensors screen window orientation rotation override", ToolTip = "Overrides the automatically tracked application-window rotation. The plugin supplies its monotonic timestamp and discovered natural orientation."))
	bool NotifyCurrentScreenRotationNative(
		EOpenMobileSensorScreenRotation Rotation
	);

	/** Use this only when you already have process-monotonic time and the device's natural orientation. Supplying wall-clock time here will break ordering. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Coordinates", meta = (DisplayName = "Update Application Window Rotation (Timestamped)", Keywords = "OpenMobile sensors screen window orientation rotation monotonic advanced", ToolTip = "Advanced override using FPlatformTime::Seconds monotonic time from this process and an explicitly known natural device orientation."))
	bool UpdateApplicationWindowRotationNative(
		EOpenMobileSensorScreenRotation Rotation,
		double TimestampSeconds,
		bool bNaturalOrientationLandscape
	);

	/** You'll only need this raw-handle path that replaces the complete options struct. Prefer focused setters on a typed sensor listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Replace Sensor Subscription Options (Advanced)", ToolTip = "Advanced raw-handle path that replaces the complete options struct. Prefer focused setters on a typed sensor listener."))
	FOpenMobileSensorOperationResult UpdateSubscriptionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorStreamOptions& Options
	);

	/** Use this to stop one owned raw subscription. An invalid or foreign handle gets an explicit result, it won't be treated as stopped. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Stop Sensor Subscription", ToolTip = "Stops one owned subscription and returns an explicit invalid-handle result when needed."))
	FOpenMobileSensorOperationResult StopSubscriptionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	/** Use this only when every raw subscription in this Game Instance must stop. Typed recording and replay sessions won't be touched. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Stop All Game Instance Sensor Subscriptions (Advanced)", Keywords = "OpenMobile sensors raw handles Game Instance stop all", ToolTip = "Advanced raw-handle cleanup. Stops every sensor subscription owned by this Game Instance, but does not stop typed recording or replay sessions. Prefer Stop Sensor Listeners for scoped typed cleanup."))
	int32 StopAllSubscriptionsNative();

	/** You'll get every unfinished typed listener owned by this Game Instance. Starting, active, and paused listeners are included. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Listeners", meta = (DisplayName = "Get Managed Sensor Listeners", Keywords = "OpenMobile sensors listeners active paused starting Game Instance collection", ToolTip = "Copies the current unfinished typed listeners owned by this Game Instance, including starting, active, and paused listeners."))
	TArray<UOpenMobileSensorListener*> GetManagedSensorListenersNative() const;

	/** You'll get one coherent current state snapshot for an owned raw subscription handle. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Sensor Subscription State", ToolTip = "Copies one coherent current state snapshot for an owned raw subscription handle."))
	bool GetSubscriptionStateNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOpenMobileSensorSubscriptionStateSnapshot& OutState
	) const;

	/** Use this for one stable cached vector read. A true Has Sample doesn't promise freshness, so inspect the read result also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Vector Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached vector snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestVectorSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileVectorSensorSample& OutSample
	) const;

	/** Use this for one stable cached attitude read. A true Has Sample doesn't promise freshness, so inspect the read result also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Attitude Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached attitude snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestAttitudeSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileAttitudeSensorSample& OutSample
	) const;

	/** Use this for one stable cached scalar read. A true Has Sample doesn't promise freshness, so inspect the read result also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Scalar Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached scalar snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestScalarSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	) const;

	/** Use this for one stable cached heading read. A true Has Sample doesn't promise freshness, so inspect the read result also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Heading Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached heading snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestHeadingSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileHeadingSensorSample& OutSample
	) const;

	/** Use this for one stable cached steps read. A true Has Sample doesn't promise freshness, so inspect the read result also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Steps Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached steps snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestStepsSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	) const;

	/** Use this for one stable cached activity read. A true Has Sample doesn't promise freshness, so inspect the read result also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Activity Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached activity snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestActivitySampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileActivitySensorSample& OutSample
	) const;

	/** Use this for one stable cached orientation read. A true Has Sample doesn't promise freshness, so inspect the read result also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Orientation Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached orientation snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestOrientationSampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileOrientationSensorSample& OutSample
	) const;

	/** Use this for one stable cached proximity read. A true Has Sample doesn't promise freshness, so inspect the read result also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Proximity Sample", ReturnDisplayName = "Has Sample", AdvancedDisplay = "LastSeenSequence", ToolTip = "Copies one coherent cached proximity snapshot. Has Sample may still be stale, paused, or invalid; inspect the read result."))
	bool GetLatestProximitySampleNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileProximitySensorSample& OutSample
	) const;

	/** Use this when every ordered vector sample is needed. Max Samples keeps one call bounded, and you'll get overflow counts also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Read Buffered Vector Samples", ToolTip = "Drains a bounded number of ordered vector samples and returns overflow counters."))
	bool GetBufferedVectorSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileVectorSensorBatch& OutBatch
	);

	/** Use this when every ordered attitude sample is needed. Max Samples keeps one call bounded, and you'll get overflow counts also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Read Buffered Attitude Samples", ToolTip = "Drains a bounded number of ordered attitude samples and returns overflow counters."))
	bool GetBufferedAttitudeSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileAttitudeSensorBatch& OutBatch
	);

	/** Use this when every ordered scalar sample is needed. Max Samples keeps one call bounded, and you'll get overflow counts also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Read Buffered Scalar Samples", ToolTip = "Drains a bounded number of ordered scalar samples and returns overflow counters."))
	bool GetBufferedScalarSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileScalarSensorBatch& OutBatch
	);

	/** Use this when every ordered heading sample is needed. Max Samples keeps one call bounded, and you'll get overflow counts also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Read Buffered Heading Samples", ToolTip = "Drains a bounded number of ordered heading samples and returns overflow counters."))
	bool GetBufferedHeadingSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileHeadingSensorBatch& OutBatch
	);

	/** Use this when every ordered steps sample is needed. Max Samples keeps one call bounded, and you'll get overflow counts also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Read Buffered Steps Samples", ToolTip = "Drains a bounded number of ordered steps samples and returns overflow counters."))
	bool GetBufferedStepsSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileStepsSensorBatch& OutBatch
	);

	/** Use this when every ordered activity sample is needed. Max Samples keeps one call bounded, and you'll get overflow counts also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Read Buffered Activity Samples", ToolTip = "Drains a bounded number of ordered activity samples and returns overflow counters."))
	bool GetBufferedActivitySamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileActivitySensorBatch& OutBatch
	);

	/** Use this when every ordered orientation sample is needed. Max Samples keeps one call bounded, and you'll get overflow counts also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Read Buffered Orientation Samples", ToolTip = "Drains a bounded number of ordered orientation samples and returns overflow counters."))
	bool GetBufferedOrientationSamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileOrientationSensorBatch& OutBatch
	);

	/** Use this when every ordered proximity sample is needed. Max Samples keeps one call bounded, and you'll get overflow counts also. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Read Buffered Proximity Samples", ToolTip = "Drains a bounded number of ordered proximity samples and returns overflow counters."))
	bool GetBufferedProximitySamplesNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		UPARAM(meta = (ClampMin = "1", ClampMax = "4096")) int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		FOpenMobileProximitySensorBatch& OutBatch
	);

	/** Use this from C++ when queued native samples must be delivered before continuing. The returned GUID can cancel the one completion callback. */
	FGuid FlushNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOnOpenMobileSensorFlushComplete&& Completion
	);
	/** Call this while a native flush is still pending. You'll get false when the request already finished or never belonged here. */
	bool CancelFlushNative(FGuid RequestId);

	/** Use this from C++ for asynchronous raw-handle recentering. Completion runs once with the provider's real support result. */
	FGuid RecenterNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode,
		FOnOpenMobileSensorRecenterComplete&& Completion
	);

	/** You'll only need this raw-handle path. Prefer Recenter Attitude or Clear Attitude Recenter on a typed attitude listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Recenter Sensor Attitude by Handle (Advanced)", ToolTip = "Advanced raw-handle path. Prefer Recenter Attitude or Clear Attitude Recenter on a typed attitude listener."))
	FOpenMobileSensorRecenterResult RecenterSubscription(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode
	);

	/** This one's the old raw-handle session. Prefer the typed relative-altitude listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Begin Relative Altitude Session by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Listen for Relative Altitude and keep the typed listener object.", ToolTip = "Legacy raw-handle session. Prefer the typed relative-altitude listener."))
	FOpenMobileSensorSubscriptionResult BeginRelativeAltitudeSessionNative(
		const FOpenMobileSensorStreamOptions& Options
	);

	/** This one's the old raw-handle recenter. Prefer the typed listener control. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Recenter Relative Altitude by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Recenter Relative Altitude on a typed relative-altitude listener.", ToolTip = "Legacy raw-handle recenter. Prefer the typed listener control."))
	FOpenMobileSensorOperationResult RecenterRelativeAltitudeBaselineNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	/** This one's the old raw-handle read. Prefer the typed listener snapshot. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Read Relative Altitude by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Get Latest Relative Altitude on a typed relative-altitude listener.", ToolTip = "Legacy raw-handle read. Prefer the typed listener snapshot."))
	bool ReadRelativeAltitudeSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	) const;

	/** This one's the old raw-handle stop. Prefer stopping the typed listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Stop Relative Altitude by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Stop Sensor Listener on the typed relative-altitude listener.", ToolTip = "Legacy raw-handle stop. Prefer stopping the typed listener."))
	FOpenMobileSensorOperationResult StopRelativeAltitudeSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	/** This one's the old raw-handle session. Prefer the typed step-count listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Begin Step Count Session by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Listen for Step Count and keep the typed listener object.", ToolTip = "Legacy raw-handle session. Prefer the typed step-count listener."))
	FOpenMobileSensorSubscriptionResult BeginStepCountSessionNative(
		const FOpenMobileSensorStreamOptions& Options
	);

	/** This one's the old raw-handle reset. Prefer the typed listener control. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Reset Step Count by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Reset Step Count on a typed step-count listener.", ToolTip = "Legacy raw-handle reset. Prefer the typed listener control."))
	FOpenMobileSensorOperationResult ResetStepCountSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	/** This one's the old raw-handle read. Prefer the typed listener snapshot. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Read Step Count by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Get Latest Step Count on a typed step-count listener.", ToolTip = "Legacy raw-handle read. Prefer the typed listener snapshot."))
	bool ReadStepCountSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	) const;

	/** This one's the old raw-handle stop. Prefer stopping the typed listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Sessions", meta = (DisplayName = "Stop Step Count by Handle (Legacy)", DeprecatedFunction, DeprecationMessage = "Use Stop Sensor Listener on the typed step-count listener.", ToolTip = "Legacy raw-handle stop. Prefer stopping the typed listener."))
	FOpenMobileSensorOperationResult StopStepCountSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	/** You'll get the lifetime and discontinuity policy used by resettable step sessions. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Step Count Session Policy", ToolTip = "Returns the lifetime and discontinuity policy used by resettable step sessions."))
	FOpenMobileStepCountSessionPolicy GetStepCountSessionPolicyNative() const;
	/** Use this from C++ for a historical native step total. The callback returns on the game thread when supported. */
	FGuid QueryNativeStepCountNative(
		const FOpenMobileNativeStepCountQuery& Query,
		FOnOpenMobileNativeStepCountQueryComplete&& Completion
	);
	/** Call this before an unfinished historical query completes. False means no matching request was still owned. */
	bool CancelNativeStepCountQueryNative(const FGuid& RequestId);

	/** You'll only need this raw-handle path. Prefer the calibration node exposed by a typed attitude, magnetometer, or heading listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Request Calibration by Handle (Advanced)", ToolTip = "Advanced raw-handle path. Prefer the calibration node exposed by a typed attitude, magnetometer, or heading listener."))
	FOpenMobileSensorOperationResult RequestNativeCalibrationPrompt(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	/** You'll get the current normalized permission state without displaying a system prompt. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Permissions", meta = (DisplayName = "Get Sensor Permission Status", ToolTip = "Copies the current normalized permission state without displaying a system prompt."))
	FOpenMobilePermissionResult GetPermissionStatusNative(
		EOpenMobileSensorPermission Permission
	) const;
	/** Call this from a user action to start one sensor-owned permission request. Completion is normalized and runs once on the game thread. */
	FOpenMobilePermissionRequestHandle RequestPermissionNative(
		EOpenMobileSensorPermission Permission,
		FOnOpenMobilePermissionRequestComplete&& Completion
	);
	/** Use this when the requesting owner disappears before the permission answer. A system dialog may remain, but its callback won't reach the dead owner. */
	bool CancelPermissionRequestNative(
		const FOpenMobilePermissionRequestHandle& Handle
	);

	/** Call this after your location provider has an authorized fresh fix. Sensors won't start location services or request location permission for you. */
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

	/** Use this only when you already have the raw location struct and Unix timestamp. Normal Blueprint graphs should pass a UTC date-time through Set True Heading Location. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Pose and Heading", meta = (DisplayName = "Set True Heading Location Input (Advanced)", ToolTip = "Advanced raw-struct path using a Unix timestamp. Prefer Set True Heading Location with a UTC date-time."))
	FOpenMobileSensorOperationResult SetTrueHeadingLocationInputNative(
		const FOpenMobileSensorLocationInput& LocationInput
	);

	/** Call this as soon as the caller-owned fix shouldn't be retained for true heading. Future samples won't use the old location. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Clear True Heading Location", ToolTip = "Immediately removes caller-owned location retained for true heading."))
	FOpenMobileSensorOperationResult ClearTrueHeadingLocationInputNative();
	/** Use this from C++ to start raw recording with explicit options. The callback confirms capture started, it doesn't mean the file is finalized. */
	FGuid StartRecordingNative(
		const FOpenMobileSensorRecordingOptions& Options,
		FOnOpenMobileSensorRecordingComplete&& Completion
	);
	/** Use this from C++ to stop capture and finalize its file. The callback is the point where replay can safely open it. */
	FGuid StopRecordingNative(
		FGuid RequestId,
		FOnOpenMobileSensorRecordingComplete&& Completion
	);
	/** Use this to cancel and discard one raw recording request. Typed sessions should call Discard Sensor Recording instead. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (DisplayName = "Cancel Sensor Recording by GUID (Advanced)", ToolTip = "Cancels and discards one raw recording request. Prefer Discard Sensor Recording on a typed session."))
	FOpenMobileSensorOperationResult CancelRecordingNative(FGuid RequestId);
	/** Typed sessions call this after their terminal event so the subsystem can drop its owner reference. It won't delete a finalized file. */
	void ReleaseRecordingSessionNative(FGuid RequestId);

	/** You'll get every sensor identifier used by a starting, active, or paused raw stream. Repeated shared streams appear once. */
	TArray<FOpenMobileSensorIdentifier>
	GetActiveSensorIdentifiersNative() const;

	/** You'll get the current owner-scoped typed recording sessions that are starting, recording, or finalizing. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Active Sensor Recordings", Keywords = "OpenMobile sensors recording session active list", ToolTip = "Copies the current owner-scoped typed recording sessions that are starting, recording, or finalizing."))
	TArray<UOpenMobileSensorRecordingSession*>
	GetActiveRecordingSessionsNative() const;
	/** Use this from C++ to start raw replay and receive its startup result once. Later playback control still uses the returned GUID. */
	FGuid ReplayRecordingNative(
		const FString& FilePath,
		const FOpenMobileSensorReplayOptions& Options,
		FOnOpenMobileSensorReplayComplete&& Completion
	);
	/** Use this to cancel one raw replay request. Prefer Stop Sensor Replay on a typed session. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Cancel Sensor Replay by GUID (Advanced)", ToolTip = "Cancels one raw replay request. Prefer Stop Sensor Replay on a typed session."))
	FOpenMobileSensorOperationResult CancelReplayNative(FGuid RequestId);

	/** You'll get the current owner-scoped typed replay sessions that are loading, playing, or paused. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Replay", meta = (DisplayName = "Get Active Sensor Replays", Keywords = "OpenMobile sensors replay session active list", ToolTip = "Copies the current owner-scoped typed replay sessions that are loading, playing, or paused."))
	TArray<UOpenMobileSensorReplaySession*>
	GetActiveReplaySessionsNative() const;

	/** Use this to pause one raw replay request without losing its playback position. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Pause Sensor Replay by GUID (Advanced)", ToolTip = "Pauses one raw replay request without losing its playback position."))
	FOpenMobileSensorOperationResult PauseReplayNative(FGuid RequestId);

	/** Use this to resume one paused raw replay request. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Resume Sensor Replay by GUID (Advanced)", ToolTip = "Resumes one paused raw replay request."))
	FOpenMobileSensorOperationResult ResumeReplayNative(FGuid RequestId);

	/** Use this to move one raw replay request to a validated recording-relative time in seconds. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Seek Sensor Replay by GUID (Advanced)", ToolTip = "Moves one raw replay request to a validated recording-relative time in seconds."))
	FOpenMobileSensorOperationResult SeekReplayNative(
		FGuid RequestId,
		double PlaybackTimeSeconds
	);

	/** Use this to change raw replay speed while preserving the current playback position. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Set Sensor Replay Speed by GUID (Advanced)", ToolTip = "Changes raw replay speed while preserving the current playback position."))
	FOpenMobileSensorOperationResult SetReplaySpeedNative(
		FGuid RequestId,
		double PlaybackSpeed
	);

	/** Use this to enable or disable looping for one raw replay request. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Set Sensor Replay Looping by GUID (Advanced)", ToolTip = "Enables or disables looping for one raw replay request."))
	FOpenMobileSensorOperationResult SetReplayLoopingNative(
		FGuid RequestId,
		bool bLoop
	);

	/** Use this to advance a raw manual-clock replay by a finite nonnegative duration in seconds. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Advance Manual Sensor Replay by GUID (Advanced)", ToolTip = "Advances a raw manual-clock replay by a finite nonnegative duration in seconds."))
	FOpenMobileSensorOperationResult AdvanceReplayNative(
		FGuid RequestId,
		double DeltaSeconds
	);

	/** You'll get one coherent current raw replay state without advancing it. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (DisplayName = "Get Sensor Replay State by GUID (Advanced)", ToolTip = "Copies one coherent current raw replay state without advancing it."))
	bool GetReplayStateNative(
		FGuid RequestId,
		FOpenMobileSensorReplaySnapshot& OutSnapshot
	) const;

	/** You'll get one coherent current diagnostics snapshot without changing sensor state. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Sensors|Advanced|Diagnostics", meta = (DisplayName = "Get Sensor Diagnostics", ToolTip = "Copies one coherent current diagnostics snapshot without changing sensor state."))
	FOpenMobileSensorDiagnosticsSnapshot GetDiagnosticsSnapshotNative() const;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Sensor Capabilities Changed", ToolTip = "Broadcast when a material sensor capability field changes."))
	FOpenMobileSensorCapabilitiesChangedDynamic OnCapabilitiesChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Sensor Subscription State Changed", ToolTip = "Broadcast when an owned subscription changes state."))
	FOpenMobileSensorSubscriptionStateChangedDynamic OnSubscriptionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Sensor Accuracy Changed", ToolTip = "Broadcast the initial accuracy state and later quality or calibration changes on the game thread."))
	FOpenMobileSensorAccuracyChangedDynamic OnAccuracyChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Sensor Calibration Changed", ToolTip = "Broadcast deduplicated calibration-required and resolution guidance on the game thread."))
	FOpenMobileSensorCalibrationChangedDynamic OnCalibrationChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "On Sensor Samples Dropped", ToolTip = "Broadcast one coalesced loss report per game-thread dispatch cycle for an affected subscription."))
	FOpenMobileSensorSamplesDroppedDynamic OnSamplesDropped;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Sensor Error", ToolTip = "Broadcast a compact runtime error for an owned sensor stream without requiring diagnostics polling."))
	FOpenMobileSensorErrorDynamic OnSensorError;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Vector Sensor Samples", ToolTip = "Broadcast a rate-capped batch of vector samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileVectorSensorBatchDynamic OnVectorSamples;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Attitude Sensor Samples", ToolTip = "Broadcast a rate-capped batch of attitude samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileAttitudeSensorBatchDynamic OnAttitudeSamples;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Scalar Sensor Samples", ToolTip = "Broadcast a rate-capped batch of scalar samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileScalarSensorBatchDynamic OnScalarSamples;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Heading Sensor Samples", ToolTip = "Broadcast a rate-capped batch of heading samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileHeadingSensorBatchDynamic OnHeadingSamples;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Steps Sensor Samples", ToolTip = "Broadcast a rate-capped batch of steps samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileStepsSensorBatchDynamic OnStepsSamples;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Activity Sensor Samples", ToolTip = "Broadcast a rate-capped batch of activity samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileActivitySensorBatchDynamic OnActivitySamples;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Orientation Sensor Samples", ToolTip = "Broadcast a rate-capped batch of physical-orientation samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileOrientationSensorBatchDynamic OnOrientationSamples;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "On Proximity Sensor Samples", ToolTip = "Broadcast a rate-capped batch of proximity samples on the game thread. Requires Delivery Mode = Event Batches."))
	FOpenMobileProximitySensorBatchDynamic OnProximitySamples;

	/** Bind here when C++ needs capability refreshes for this Game Instance. The initial snapshot still comes from Get Capability Snapshot. */
	FOnOpenMobileSensorCapabilitiesChanged& OnCapabilitiesChangedNative();

	/** Bind here when C++ needs raw subscription-state changes. You'll receive handles owned by this Game Instance only. */
	FOnOpenMobileSensorSubscriptionStateChanged& OnSubscriptionStateChangedNative();

	/** Bind here for initial and changed accuracy reports on the game thread. */
	FOnOpenMobileSensorAccuracyChanged& OnAccuracyChangedNative();

	/** Bind here for deduplicated calibration requirement changes. The event includes the suggested user action also. */
	FOnOpenMobileSensorCalibrationChanged& OnCalibrationChangedNative();

	/** Bind here when C++ needs coalesced queue-loss reports. Each report identifies the affected subscription. */
	FOnOpenMobileSensorSamplesDropped& OnSamplesDroppedNative();

	/** Bind here for normalized runtime stream errors. You won't need to poll the diagnostics snapshot. */
	FOnOpenMobileSensorError& OnSensorErrorNative();

	/** Bind here for raw vector batches. The subscription must request Event Batches. */
	FOnOpenMobileVectorSensorBatch& OnVectorSamplesNative();

	/** Bind here for raw attitude batches. The subscription must request Event Batches. */
	FOnOpenMobileAttitudeSensorBatch& OnAttitudeSamplesNative();

	/** Bind here for raw scalar batches. The subscription must request Event Batches. */
	FOnOpenMobileScalarSensorBatch& OnScalarSamplesNative();

	/** Bind here for raw heading batches. The subscription must request Event Batches. */
	FOnOpenMobileHeadingSensorBatch& OnHeadingSamplesNative();

	/** Bind here for raw step-family batches. The subscription must request Event Batches. */
	FOnOpenMobileStepsSensorBatch& OnStepsSamplesNative();

	/** Bind here for raw activity-family batches. The subscription must request Event Batches. */
	FOnOpenMobileActivitySensorBatch& OnActivitySamplesNative();

	/** Bind here for raw physical-orientation batches. The subscription must request Event Batches. */
	FOnOpenMobileOrientationSensorBatch& OnOrientationSamplesNative();

	/** Bind here for raw proximity batches. The subscription must request Event Batches. */
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

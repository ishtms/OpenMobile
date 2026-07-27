#pragma once

#include "CoreMinimal.h"
#include "OpenMobileNativeStepCount.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorCalibration.h"
#include "OpenMobileSensorDiagnostics.h"
#include "OpenMobileSensorMetadata.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileStepCountSession.h"
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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Update Application Window Rotation", ToolTip = "Supplies a timestamped application-window rotation for current-screen sensor subscriptions."))
	bool UpdateApplicationWindowRotationNative(
		EOpenMobileSensorScreenRotation Rotation,
		double TimestampSeconds,
		bool bNaturalOrientationLandscape
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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Recenter Sensor Attitude", ToolTip = "Applies full-attitude, yaw-only, or clear recentering to one attitude subscription without restarting its physical stream."))
	FOpenMobileSensorRecenterResult RecenterSubscription(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorRecenterMode Mode
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Begin Relative Altitude Session", ToolTip = "Begins an owner-scoped relative-altitude session with an independent zero baseline."))
	FOpenMobileSensorSubscriptionResult BeginRelativeAltitudeSessionNative(
		const FOpenMobileSensorStreamOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Recenter Relative Altitude Baseline", ToolTip = "Clears one session baseline so its next accepted sample becomes zero metres."))
	FOpenMobileSensorOperationResult RecenterRelativeAltitudeBaselineNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Relative Altitude Session", ToolTip = "Reads the latest relative-altitude sample for one owned session."))
	bool ReadRelativeAltitudeSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Stop Relative Altitude Session", ToolTip = "Stops one owned relative-altitude session without affecting another session sharing its pressure stream."))
	FOpenMobileSensorOperationResult StopRelativeAltitudeSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Begin Step Count Session", ToolTip = "Begins an owner-scoped step session whose first accepted native total becomes its baseline."))
	FOpenMobileSensorSubscriptionResult BeginStepCountSessionNative(
		const FOpenMobileSensorStreamOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Reset Step Count Session", ToolTip = "Clears one session so its next accepted native total becomes a new zero baseline."))
	FOpenMobileSensorOperationResult ResetStepCountSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Read Step Count Session", ToolTip = "Reads the latest nonnegative count for one owned resettable step session."))
	bool ReadStepCountSessionNative(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Stop Step Count Session", ToolTip = "Stops one owned session without resetting other sessions or the platform native total."))
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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Request Native Sensor Calibration Prompt", ToolTip = "Explicitly asks the active backend to show a native calibration prompt when supported."))
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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Set True Heading Location Input", ToolTip = "Supplies narrow caller-owned location input without starting location services."))
	FOpenMobileSensorOperationResult SetTrueHeadingLocationInputNative(
		const FOpenMobileSensorLocationInput& LocationInput
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Clear True Heading Location Input", ToolTip = "Immediately removes caller-owned location retained for true heading."))
	FOpenMobileSensorOperationResult ClearTrueHeadingLocationInputNative();
	FGuid StartRecordingNative(
		const FOpenMobileSensorRecordingOptions& Options,
		FOnOpenMobileSensorRecordingComplete&& Completion
	);
	FGuid StopRecordingNative(
		FGuid RequestId,
		FOnOpenMobileSensorRecordingComplete&& Completion
	);
	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Cancel Sensor Recording", ToolTip = "Cancels and discards one owned sensor recording."))
	FOpenMobileSensorOperationResult CancelRecordingNative(FGuid RequestId);
	FGuid ReplayRecordingNative(
		const FString& FilePath,
		const FOpenMobileSensorReplayOptions& Options,
		FOnOpenMobileSensorReplayComplete&& Completion
	);
	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Cancel Sensor Replay", ToolTip = "Cancels one owned sensor replay and completes it exactly once."))
	FOpenMobileSensorOperationResult CancelReplayNative(FGuid RequestId);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Pause Sensor Replay", ToolTip = "Pauses one owned replay without losing its playback position."))
	FOpenMobileSensorOperationResult PauseReplayNative(FGuid RequestId);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Resume Sensor Replay", ToolTip = "Resumes one paused owned replay."))
	FOpenMobileSensorOperationResult ResumeReplayNative(FGuid RequestId);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Seek Sensor Replay", ToolTip = "Moves one owned replay to a validated recording-relative time."))
	FOpenMobileSensorOperationResult SeekReplayNative(
		FGuid RequestId,
		double PlaybackTimeSeconds
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Set Sensor Replay Speed", ToolTip = "Changes replay speed while preserving the current playback position."))
	FOpenMobileSensorOperationResult SetReplaySpeedNative(
		FGuid RequestId,
		double PlaybackSpeed
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Set Sensor Replay Looping", ToolTip = "Enables or disables looping for one owned replay."))
	FOpenMobileSensorOperationResult SetReplayLoopingNative(
		FGuid RequestId,
		bool bLoop
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors", meta = (DisplayName = "Advance Manual Sensor Replay", ToolTip = "Advances a manual-clock replay by a finite nonnegative duration."))
	FOpenMobileSensorOperationResult AdvanceReplayNative(
		FGuid RequestId,
		double DeltaSeconds
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Sensor Replay State", ToolTip = "Returns one owned replay state without advancing it."))
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

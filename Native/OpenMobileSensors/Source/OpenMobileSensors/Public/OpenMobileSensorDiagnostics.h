#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorAccuracy.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorErrorReport.h"
#include "OpenMobileSensorMetadata.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorSubscription.h"
#include "OpenMobileSensorDiagnostics.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorBatchingMode : uint8
{
	Unknown UMETA(DisplayName = "Unknown", ToolTip = "The active backend did not report how samples are batched."),
	Disabled UMETA(DisplayName = "Batching Disabled", ToolTip = "Samples are delivered without native or plugin batching."),
	Native UMETA(DisplayName = "Native Batching", ToolTip = "The platform sensor API batches samples before delivery."),
	Plugin UMETA(DisplayName = "Plugin Batching", ToolTip = "OpenMobile Sensors batches samples after receiving them from the platform."),
	Unavailable UMETA(DisplayName = "Batching Unavailable", ToolTip = "The requested batching behavior is unavailable for this stream.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRateDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sample Count for this sensor rate diagnostics."))
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Requested frequency in hertz for this sensor rate diagnostics."))
	double RequestedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Applied frequency in hertz for this sensor rate diagnostics."))
	double AppliedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Mean frequency in hertz for this sensor rate diagnostics."))
	double MeanFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Interval jitter in seconds for this sensor rate diagnostics."))
	double IntervalJitterSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Last gap in seconds for this sensor rate diagnostics."))
	double LastGapSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorGyroscopeDriftDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sample Count for this sensor gyroscope drift diagnostics."))
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Mean angular velocity in radians per second for this sensor gyroscope drift diagnostics."))
	FVector MeanAngularVelocityRadiansPerSecond = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Root mean square angular speed in radians per second for this sensor gyroscope drift diagnostics."))
	double RootMeanSquareAngularSpeedRadiansPerSecond = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorStreamDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Subscription for this sensor stream diagnostics."))
	FOpenMobileSensorSubscriptionStateSnapshot Subscription;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Rate for this sensor stream diagnostics."))
	FOpenMobileSensorRateDiagnostics Rate;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Gyroscope Drift for this sensor stream diagnostics."))
	FOpenMobileSensorGyroscopeDriftDiagnostics GyroscopeDrift;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Batching Mode for this sensor stream diagnostics."))
	EOpenMobileSensorBatchingMode BatchingMode =
		EOpenMobileSensorBatchingMode::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Queue Depth for this sensor stream diagnostics."))
	int32 QueueDepth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Buffer High Water Mark for this sensor stream diagnostics."))
	int32 BufferHighWaterMark = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Dropped Samples for this sensor stream diagnostics."))
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Filtered Samples for this sensor stream diagnostics."))
	int64 FilteredSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Suppressed Samples for this sensor stream diagnostics."))
	int64 SuppressedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Latest sample age in seconds for this sensor stream diagnostics."))
	double LatestSampleAgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Queue delay in seconds for this sensor stream diagnostics."))
	double QueueDelaySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Game thread processing in seconds for this sensor stream diagnostics."))
	double GameThreadProcessingSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when sample is present and safe to read."))
	bool bHasSample = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorSourceFlags", ToolTip = "Semantic source flags for the latest sample. Use Has Sensor Source for normal branching."))
	int32 SourceFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when accuracy is present and safe to read."))
	bool bHasAccuracy = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Provider-reported accuracy classification for this value."))
	FOpenMobileSensorAccuracySnapshot Accuracy;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorPhysicalStreamDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor type and provider instance this value describes."))
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Backend Name for this sensor physical stream diagnostics."))
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Attitude Reference Frame for this sensor physical stream diagnostics."))
	EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Subscriber Count for this sensor physical stream diagnostics."))
	int32 SubscriberCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Applied frequency in hertz for this sensor physical stream diagnostics."))
	double AppliedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Maximum delivery latency in seconds for this sensor physical stream diagnostics."))
	double MaximumDeliveryLatencySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Diagnostics|Physical Stream", meta = (ToolTip = "Whether any subscriber caused the provider to request its lowest practical delivery latency."))
	bool bLowLatency = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Diagnostics|Physical Stream", meta = (ToolTip = "Whether one or more subscribers requested a nonzero native delivery latency."))
	bool bNativeBatchingRequested = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Diagnostics|Physical Stream", meta = (ToolTip = "Whether the active platform provider confirmed that native batching is in use."))
	bool bNativeBatchingApplied = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorDiagnosticsSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "UTC date and time when this diagnostics snapshot was captured."))
	FString CapturedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Backend Name for this sensor diagnostics snapshot."))
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Backend Availability for this sensor diagnostics snapshot."))
	FOpenMobileCapability BackendAvailability;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Backend Generation for this sensor diagnostics snapshot."))
	int64 BackendGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Capabilities for this sensor diagnostics snapshot."))
	TArray<FOpenMobileSensorCapability> Capabilities;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Metadata for this sensor diagnostics snapshot."))
	TArray<FOpenMobileSensorMetadata> Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Permissions for this sensor diagnostics snapshot."))
	TArray<FOpenMobileSensorPermissionDescriptor> Permissions;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Streams for this sensor diagnostics snapshot."))
	TArray<FOpenMobileSensorStreamDiagnostics> Streams;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Physical Streams for this sensor diagnostics snapshot."))
	TArray<FOpenMobileSensorPhysicalStreamDiagnostics> PhysicalStreams;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Active Recording Count for this sensor diagnostics snapshot."))
	int32 ActiveRecordingCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Active Replay Count for this sensor diagnostics snapshot."))
	int32 ActiveReplayCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Recent Errors for this sensor diagnostics snapshot."))
	TArray<FOpenMobileError> RecentErrors;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Recent Error Reports for this sensor diagnostics snapshot."))
	TArray<FOpenMobileSensorErrorReport> RecentErrorReports;
};

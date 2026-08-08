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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double RequestedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double AppliedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double MeanFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double IntervalJitterSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double LastGapSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorGyroscopeDriftDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FVector MeanAngularVelocityRadiansPerSecond = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double RootMeanSquareAngularSpeedRadiansPerSecond = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorStreamDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSubscriptionStateSnapshot Subscription;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorRateDiagnostics Rate;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorGyroscopeDriftDiagnostics GyroscopeDrift;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorBatchingMode BatchingMode =
		EOpenMobileSensorBatchingMode::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 QueueDepth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 BufferHighWaterMark = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 FilteredSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 SuppressedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double LatestSampleAgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double QueueDelaySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double GameThreadProcessingSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasSample = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorSourceFlags", ToolTip = "Semantic source flags for the latest sample. Use Has Sensor Source for normal branching."))
	int32 SourceFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasAccuracy = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorAccuracySnapshot Accuracy;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorPhysicalStreamDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 SubscriberCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double AppliedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double MaximumDeliveryLatencySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bLowLatency = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bNativeBatchingRequested = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bNativeBatchingApplied = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorDiagnosticsSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FString CapturedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileCapability BackendAvailability;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 BackendGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorCapability> Capabilities;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorMetadata> Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorPermissionDescriptor> Permissions;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorStreamDiagnostics> Streams;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorPhysicalStreamDiagnostics> PhysicalStreams;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 ActiveRecordingCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 ActiveReplayCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileError> RecentErrors;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorErrorReport> RecentErrorReports;
};

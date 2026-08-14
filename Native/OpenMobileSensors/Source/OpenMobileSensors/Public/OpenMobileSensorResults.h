#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorErrors.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorRequestHandles.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorSubscription.h"
#include "OpenMobileSensorResults.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorResultCode : uint8
{
	Success UMETA(DisplayName = "Success", ToolTip = "The operation completed successfully."),
	Accepted UMETA(DisplayName = "Accepted, Starting Asynchronously", ToolTip = "The request was accepted and its final state will arrive asynchronously."),
	NotSupported UMETA(DisplayName = "Not Supported", ToolTip = "The active platform or provider does not support this operation."),
	InvalidHandle UMETA(DisplayName = "Invalid Handle", ToolTip = "The supplied subscription or session handle is invalid or stale."),
	InvalidArgument UMETA(DisplayName = "Invalid Argument", ToolTip = "One or more request fields are invalid or inconsistent."),
	Unavailable UMETA(DisplayName = "Unavailable", ToolTip = "The operation is valid but its required sensor or service is currently unavailable."),
	Cancelled UMETA(DisplayName = "Cancelled", ToolTip = "The operation was cancelled before successful completion."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "The operation failed for the reason and message returned with the result.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results", meta = (ToolTip = "Top-level synchronous or terminal outcome. Accepted means startup continues asynchronously and is not a failure."))
	EOpenMobileSensorResultCode Code = EOpenMobileSensorResultCode::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Structured sensor failure reason and native provider context."))
	FOpenMobileSensorFailureDetails Failure;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Stable error name and developer-facing diagnostic detail."))
	FOpenMobileError Error;

	/** Use this for normal result checks because Accepted is a valid asynchronous start also. Looking for Success alone will reject good requests. */
	bool IsSuccess() const
	{
		return Code == EOpenMobileSensorResultCode::Success
			|| Code == EOpenMobileSensorResultCode::Accepted;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorSubscriptionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Outcome details for the operation, including any failure and correction."))
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Typed identifier for the subscription or session this value describes."))
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Stream options requested by the caller before validation and rate resolution."))
	FOpenMobileSensorStreamOptions RequestedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Validated stream options currently applied by the provider."))
	FOpenMobileSensorStreamOptions AppliedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Requested, clamped, and native rates with any adjustment explanation."))
	FOpenMobileSensorRateResolution RateResolution;
};

UENUM(BlueprintType)
enum class EOpenMobileSensorReadStatus : uint8
{
	NoSample UMETA(DisplayName = "No Sample Yet", ToolTip = "The stream is valid but has not accepted its first sample."),
	Valid UMETA(DisplayName = "Valid Sample", ToolTip = "The latest sample exists and is within its rate-derived freshness window."),
	Stale UMETA(DisplayName = "Stale Sample", ToolTip = "A cached sample exists but is older than the stream's freshness window."),
	Paused UMETA(DisplayName = "Stream Paused", ToolTip = "The stream is paused and its cached sample is not currently updating."),
	Stopped UMETA(DisplayName = "Stream Stopped", ToolTip = "The stream has reached a terminal stopped state."),
	InvalidHandle UMETA(DisplayName = "Invalid Handle", ToolTip = "The supplied listener or subscription handle is invalid or stale.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results|Read", meta = (ToolTip = "Explains whether a cached sample exists, whether it is fresh, and whether the stream can still update it."))
	EOpenMobileSensorReadStatus Status = EOpenMobileSensorReadStatus::NoSample;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results|Read", meta = (ToolTip = "Age in seconds from the sample's monotonic sensor timestamp to the read time. Zero when no sample exists."))
	double SampleAgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results|Read", meta = (ToolTip = "Whether Sequence is newer than the previous sequence supplied by the caller."))
	bool bHasNewerSample = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonically increasing sequence used to order values from this source."))
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results|Read", meta = (ToolTip = "Validity of the cached sample itself. A stale sample may still be structurally valid."))
	bool bSampleValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Provider-reported accuracy classification for this value."))
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Typed provenance flags describing how the value was produced.", Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorSourceFlags"))
	int32 SourceFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Stable error name and developer-facing diagnostic detail."))
	FOpenMobileError Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorBufferReadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Outcome details for the operation, including any failure and correction."))
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results|Buffer", meta = (ToolTip = "Number of samples copied into the caller's output batch by this read."))
	int32 ReturnedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results|Buffer", meta = (ToolTip = "Cumulative samples lost by this subscription because its bounded buffer overflowed."))
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results|Buffer", meta = (ToolTip = "Largest queue depth reached by this subscription since it started."))
	int32 BufferHighWaterMark = 0;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Sample Drop Info"))
struct OPENMOBILESENSORS_API FOpenMobileSensorDropInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Delivery path that lost samples."))
	EOpenMobileSensorDeliveryMode DeliveryMode =
		EOpenMobileSensorDeliveryMode::LatestValue;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples lost since the previous notification for this subscription."))
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Cumulative samples lost by this subscription."))
	int64 TotalDroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Policy used when the bounded sample queue became full."))
	EOpenMobileSensorOverflowPolicy OverflowPolicy =
		EOpenMobileSensorOverflowPolicy::DropOldest;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorFlushResult
{
	GENERATED_BODY()

	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Typed identity for this flush request. Different request-handle types cannot be connected in Blueprint."))
	FOpenMobileSensorFlushHandle Request;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Typed identifier for the subscription or session this value describes."))
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Outcome details for the operation, including any failure and correction."))
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Results|Flush", meta = (ToolTip = "Number of provider-held samples made available by this completed flush."))
	int32 FlushedSamples = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecenterResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced", meta = (AdvancedDisplay, ToolTip = "Raw recenter request GUID retained for compatibility. Preferred typed listener controls do not require it."))
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Typed identifier for the subscription or session this value describes."))
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Mode for this sensor recenter result."))
	EOpenMobileSensorRecenterMode Mode =
		EOpenMobileSensorRecenterMode::FullAttitude;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Outcome details for the operation, including any failure and correction."))
	FOpenMobileSensorOperationResult Operation;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecordingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Outcome details for the operation, including any failure and correction."))
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Recording for this sensor recording result."))
	FOpenMobileSensorRecordingSnapshot Recording;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReplayResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Outcome details for the operation, including any failure and correction."))
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced", meta = (AdvancedDisplay, ToolTip = "Raw replay request GUID retained for compatibility. Prefer a typed replay session object."))
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Playback time in seconds for this sensor replay result."))
	double PlaybackTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "File Status for this sensor replay result."))
	EOpenMobileSensorRecordingDecodeStatus FileStatus =
		EOpenMobileSensorRecordingDecodeStatus::NotChecked;
};

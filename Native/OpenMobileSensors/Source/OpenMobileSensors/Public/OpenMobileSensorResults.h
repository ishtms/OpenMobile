#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorErrors.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorSubscription.h"
#include "OpenMobileSensorResults.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorResultCode : uint8
{
	Success,
	Accepted,
	NotSupported,
	InvalidHandle,
	InvalidArgument,
	Unavailable,
	Cancelled,
	Failed
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorResultCode Code = EOpenMobileSensorResultCode::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorFailureDetails Failure;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileError Error;

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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorStreamOptions RequestedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorStreamOptions AppliedOptions;
};

UENUM(BlueprintType)
enum class EOpenMobileSensorReadStatus : uint8
{
	NoSample,
	Valid,
	Stale,
	Paused,
	Stopped,
	InvalidHandle
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorReadStatus Status = EOpenMobileSensorReadStatus::NoSample;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double SampleAgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasNewerSample = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileError Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorBufferReadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int32 ReturnedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int32 BufferHighWaterMark = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorFlushResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int32 FlushedSamples = 0;
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRecenterMode : uint8
{
	FullAttitude,
	YawOnly,
	Clear
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecenterResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorRecenterMode Mode =
		EOpenMobileSensorRecenterMode::FullAttitude;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOperationResult Operation;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecordingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorRecordingSnapshot Recording;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReplayResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double PlaybackTimeSeconds = 0.0;
};

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorSubscription.h"
#include "OpenMobileSensorDiagnostics.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorBatchingMode : uint8
{
	Unknown,
	Disabled,
	Native,
	Plugin,
	Unavailable
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRateDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double RequestedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double AppliedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double MeanFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double IntervalJitterSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double LastGapSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorGyroscopeDriftDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FVector MeanAngularVelocityRadiansPerSecond = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double RootMeanSquareAngularSpeedRadiansPerSecond = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorStreamDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSubscriptionStateSnapshot Subscription;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorRateDiagnostics Rate;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorGyroscopeDriftDiagnostics GyroscopeDrift;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorBatchingMode BatchingMode =
		EOpenMobileSensorBatchingMode::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int32 QueueDepth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int32 BufferHighWaterMark = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 FilteredSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 SuppressedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double LatestSampleAgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double QueueDelaySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double GameThreadProcessingSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorDiagnosticsSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 BackendGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileSensorCapability> Capabilities;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileSensorStreamDiagnostics> Streams;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileError> RecentErrors;
};

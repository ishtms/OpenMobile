#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorRecording.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorRecordingState : uint8
{
	Idle,
	Starting,
	Recording,
	Stopping,
	Completed,
	Failed,
	Cancelled
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecordingOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileSensorIdentifier> Sensors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double MaximumDurationSeconds = 300.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	int64 MaximumBytes = 64ll * 1024 * 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bIncludeSensitiveLocationContext = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorLifecyclePolicy LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::SuspendInBackground;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReplayOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double PlaybackSpeed = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double StartTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecordingSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorRecordingState State =
		EOpenMobileSensorRecordingState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int32 FormatVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 BytesWritten = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FString FilePath;
};

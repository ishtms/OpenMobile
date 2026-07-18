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

UENUM(BlueprintType)
enum class EOpenMobileSensorReplayClockMode : uint8
{
	RealTime,
	Manual
};

UENUM(BlueprintType)
enum class EOpenMobileSensorReplayState : uint8
{
	Invalid,
	Loading,
	Playing,
	Paused
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRecordingDecodeStatus : uint8
{
	NotChecked,
	Success,
	InvalidMagic,
	IncompatibleVersion,
	Truncated,
	ChecksumMismatch,
	InvalidData,
	LimitExceeded
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ToolTip = "Includes caller-supplied location only when the separate development project opt-in is enabled. Always blocked in Shipping."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bStartPaused = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorReplayClockMode ClockMode =
		EOpenMobileSensorReplayClockMode::RealTime;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReplaySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorReplayState State = EOpenMobileSensorReplayState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double PlaybackTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double PlaybackSpeed = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bLoop = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorReplayClockMode ClockMode =
		EOpenMobileSensorReplayClockMode::RealTime;
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
	int32 FormatVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bContainsSensitiveLocationContext = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 BytesWritten = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FString FilePath;
};

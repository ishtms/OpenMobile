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
enum class EOpenMobileSensorRecordingLimitReason : uint8
{
	None UMETA(DisplayName = "No Limit", ToolTip = "The recording ended explicitly or failed."),
	Duration UMETA(DisplayName = "Duration Limit", ToolTip = "The configured maximum recording duration was reached."),
	FileSize UMETA(DisplayName = "File Size Limit", ToolTip = "The configured maximum recording byte size was reached.")
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
	Paused,
	Completed,
	Failed,
	Cancelled
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorIdentifier> Sensors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	double MaximumDurationSeconds = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	int64 MaximumBytes = 1024ll * 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Includes caller-supplied location only when the separate development project opt-in is enabled. Always blocked in Shipping."))
	bool bIncludeSensitiveLocationContext = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileSensorLifecyclePolicy LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::SuspendInBackground;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReplayOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	double PlaybackSpeed = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	bool bStartPaused = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileSensorReplayClockMode ClockMode =
		EOpenMobileSensorReplayClockMode::RealTime;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReplaySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (AdvancedDisplay, ToolTip = "Raw replay request GUID retained for compatibility. Prefer the typed replay session object."))
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorReplayState State = EOpenMobileSensorReplayState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double PlaybackTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double PlaybackSpeed = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bLoop = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorReplayClockMode ClockMode =
		EOpenMobileSensorReplayClockMode::RealTime;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecordingSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (AdvancedDisplay, ToolTip = "Raw recording request GUID retained for compatibility. Prefer the typed recording session object."))
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorRecordingState State =
		EOpenMobileSensorRecordingState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 FormatVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bContainsSensitiveLocationContext = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 BytesWritten = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FString FilePath;
};

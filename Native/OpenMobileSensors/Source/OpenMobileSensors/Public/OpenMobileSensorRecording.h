#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorRecording.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorRecordingState : uint8
{
	Idle UMETA(DisplayName = "Idle", ToolTip = "The recording session has not started."),
	Starting UMETA(DisplayName = "Starting", ToolTip = "The recording request was accepted and is opening its output file."),
	Recording UMETA(DisplayName = "Recording", ToolTip = "The session is actively writing accepted sensor samples."),
	Stopping UMETA(DisplayName = "Stopping", ToolTip = "The session is finalizing its file after a stop request or configured limit."),
	Completed UMETA(DisplayName = "Completed", ToolTip = "The recording file was finalized successfully."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "The recording could not start or finalize successfully."),
	Cancelled UMETA(DisplayName = "Cancelled", ToolTip = "The recording was cancelled before successful completion.")
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
	RealTime UMETA(DisplayName = "Real Time", ToolTip = "Advances replay from engine time using the selected playback speed."),
	Manual UMETA(DisplayName = "Manual Clock", ToolTip = "Advances replay only when Blueprint explicitly steps the typed replay session.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorReplayState : uint8
{
	Invalid UMETA(DisplayName = "Invalid", ToolTip = "The replay session does not reference a valid loaded recording."),
	Loading UMETA(DisplayName = "Loading", ToolTip = "The recording is being opened and validated."),
	Playing UMETA(DisplayName = "Playing", ToolTip = "The replay clock is advancing and delivering recorded samples."),
	Paused UMETA(DisplayName = "Paused", ToolTip = "The replay remains loaded but its clock is not advancing."),
	Completed UMETA(DisplayName = "Completed", ToolTip = "Replay reached the end of the recording without looping."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "The recording could not be loaded or replayed."),
	Cancelled UMETA(DisplayName = "Cancelled", ToolTip = "Replay was cancelled before normal completion.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRecordingDecodeStatus : uint8
{
	NotChecked UMETA(DisplayName = "Not Checked", ToolTip = "The recording file has not been decoded or validated."),
	Success UMETA(DisplayName = "Valid Recording", ToolTip = "The recording passed format, checksum, and data validation."),
	InvalidMagic UMETA(DisplayName = "Invalid File Header", ToolTip = "The file does not begin with the OpenMobile Sensors recording signature."),
	IncompatibleVersion UMETA(DisplayName = "Incompatible Version", ToolTip = "The recording version is not supported by this plugin build."),
	Truncated UMETA(DisplayName = "Truncated File", ToolTip = "The file ended before its declared recording data was complete."),
	ChecksumMismatch UMETA(DisplayName = "Checksum Mismatch", ToolTip = "The recording contents do not match the stored integrity checksum."),
	InvalidData UMETA(DisplayName = "Invalid Data", ToolTip = "The file structure or one of its sample records is invalid."),
	LimitExceeded UMETA(DisplayName = "Safety Limit Exceeded", ToolTip = "The recording exceeds a configured decode size or sample-count safety limit.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecordingOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensors for this sensor recording options."))
	TArray<FOpenMobileSensorIdentifier> Sensors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Maximum duration in seconds for this sensor recording options."))
	double MaximumDurationSeconds = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Maximum in bytes for this sensor recording options."))
	int64 MaximumBytes = 1024ll * 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Includes caller-supplied location only when the separate development project opt-in is enabled. Always blocked in Shipping."))
	bool bIncludeSensitiveLocationContext = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Lifecycle Policy for this sensor recording options."))
	EOpenMobileSensorLifecyclePolicy LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::SuspendInBackground;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReplayOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Playback Speed for this sensor replay options."))
	double PlaybackSpeed = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Replay", meta = (ToolTip = "Whether replay returns to Start Time Seconds after reaching the recording end."))
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Start time in seconds for this sensor replay options."))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Replay", meta = (ToolTip = "Whether the replay loads at Start Time Seconds without advancing until resumed or manually stepped."))
	bool bStartPaused = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Clock Mode for this sensor replay options."))
	EOpenMobileSensorReplayClockMode ClockMode =
		EOpenMobileSensorReplayClockMode::RealTime;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorReplaySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced|Replay", meta = (AdvancedDisplay, ToolTip = "Raw replay request GUID retained for compatibility. Prefer the typed replay session object."))
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Current lifecycle state reported for this value."))
	EOpenMobileSensorReplayState State = EOpenMobileSensorReplayState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Playback time in seconds for this sensor replay snapshot."))
	double PlaybackTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Duration in seconds for this sensor replay snapshot."))
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Playback Speed for this sensor replay snapshot."))
	double PlaybackSpeed = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Replay", meta = (ToolTip = "Whether this replay wraps to its configured start after the recording end."))
	bool bLoop = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Clock Mode for this sensor replay snapshot."))
	EOpenMobileSensorReplayClockMode ClockMode =
		EOpenMobileSensorReplayClockMode::RealTime;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecordingSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced|Recording", meta = (AdvancedDisplay, ToolTip = "Raw recording request GUID retained for compatibility. Prefer the typed recording session object."))
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Current lifecycle state reported for this value."))
	EOpenMobileSensorRecordingState State =
		EOpenMobileSensorRecordingState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Format Version for this sensor recording snapshot."))
	int32 FormatVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Recording", meta = (ToolTip = "Whether the recording stores caller-supplied location context used for true heading."))
	bool bContainsSensitiveLocationContext = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Duration in seconds for this sensor recording snapshot."))
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Bytes Written for this sensor recording snapshot."))
	int64 BytesWritten = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Dropped Samples for this sensor recording snapshot."))
	int64 DroppedSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Absolute path of the recording file used by this session."))
	FString FilePath;
};

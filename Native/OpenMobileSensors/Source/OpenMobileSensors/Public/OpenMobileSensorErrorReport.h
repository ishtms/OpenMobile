#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorErrorReport.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Error"))
struct OPENMOBILESENSORS_API FOpenMobileSensorRuntimeError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Portable reason suitable for Blueprint control flow."))
	EOpenMobileSensorFailureReason Reason =
		EOpenMobileSensorFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Short user-facing explanation of what failed."))
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Short corrective action suitable for UI or recovery logic."))
	FText Correction;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Whether retrying after the stated correction or a temporary state change can succeed."))
	bool bRetryable = false;
};

UENUM(BlueprintType)
enum class EOpenMobileSensorOperation : uint8
{
	Unknown UMETA(DisplayName = "Unknown Operation", ToolTip = "The failing operation could not be identified."),
	CapabilityQuery UMETA(DisplayName = "Capability Query", ToolTip = "The error occurred while discovering sensor availability or metadata."),
	StartStream UMETA(DisplayName = "Start Stream", ToolTip = "The error occurred while accepting or starting a sensor stream."),
	ReconfigureStream UMETA(DisplayName = "Reconfigure Stream", ToolTip = "The error occurred while changing options on an active stream."),
	StopStream UMETA(DisplayName = "Stop Stream", ToolTip = "The error occurred while stopping a sensor stream."),
	ReadLatest UMETA(DisplayName = "Read Latest Sample", ToolTip = "The error occurred while reading the latest cached sample."),
	ReadBuffer UMETA(DisplayName = "Read Buffered Samples", ToolTip = "The error occurred while reading buffered sensor samples."),
	Flush UMETA(DisplayName = "Flush Samples", ToolTip = "The error occurred while requesting or completing a sensor flush."),
	Permission UMETA(DisplayName = "Permission", ToolTip = "The error occurred during sensor permission discovery or a permission request."),
	Calibration UMETA(DisplayName = "Calibration", ToolTip = "The error occurred during a sensor calibration workflow."),
	Recenter UMETA(DisplayName = "Recenter", ToolTip = "The error occurred while recentering attitude or relative altitude."),
	Recording UMETA(DisplayName = "Recording", ToolTip = "The error occurred while starting, running, or stopping a recording."),
	Replay UMETA(DisplayName = "Replay", ToolTip = "The error occurred while loading or controlling a recorded stream."),
	Lifecycle UMETA(DisplayName = "Application Lifecycle", ToolTip = "The error was caused by a foreground, background, pause, or resume transition."),
	BackendCallback UMETA(DisplayName = "Provider Callback", ToolTip = "The active sensor provider reported an asynchronous runtime failure.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorErrorContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor type and provider instance this value describes."))
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Outcome details for the operation, including any failure and correction."))
	EOpenMobileSensorOperation Operation = EOpenMobileSensorOperation::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Subscription Identifier for this sensor error context."))
	FGuid SubscriptionIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Backend Name for this sensor error context."))
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when rate context is present and safe to read."))
	bool bHasRateContext = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Requested frequency in hertz for this sensor error context."))
	double RequestedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Applied frequency in hertz for this sensor error context."))
	double AppliedFrequencyHz = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorErrorReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonic sensor-service timestamp in seconds for this value."))
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Context for this sensor error report."))
	FOpenMobileSensorErrorContext Context;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Result Code for this sensor error report."))
	EOpenMobileSensorResultCode ResultCode = EOpenMobileSensorResultCode::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Structured sensor failure reason and native provider context."))
	FOpenMobileSensorFailureDetails Failure;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Stable error name and developer-facing diagnostic detail."))
	FOpenMobileError Error;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Summary for this sensor error report."))
	FText Summary;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Likely Cause for this sensor error report."))
	FText LikelyCause;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Suggested action that can correct or avoid the reported problem."))
	FText Correction;
};

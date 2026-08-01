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
	Unknown,
	CapabilityQuery,
	StartStream,
	ReconfigureStream,
	StopStream,
	ReadLatest,
	ReadBuffer,
	Flush,
	Permission,
	Calibration,
	Recenter,
	Recording,
	Replay,
	Lifecycle,
	BackendCallback
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorErrorContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorOperation Operation = EOpenMobileSensorOperation::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FGuid SubscriptionIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasRateContext = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double RequestedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double AppliedFrequencyHz = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorErrorReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorErrorContext Context;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorResultCode ResultCode = EOpenMobileSensorResultCode::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorFailureDetails Failure;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileError Error;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FText Summary;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FText LikelyCause;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FText Correction;
};

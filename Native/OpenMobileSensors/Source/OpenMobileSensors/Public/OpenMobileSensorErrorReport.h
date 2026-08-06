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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorOperation Operation = EOpenMobileSensorOperation::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FGuid SubscriptionIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasRateContext = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double RequestedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double AppliedFrequencyHz = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorErrorReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorErrorContext Context;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorResultCode ResultCode = EOpenMobileSensorResultCode::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorFailureDetails Failure;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileError Error;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FText Summary;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FText LikelyCause;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FText Correction;
};

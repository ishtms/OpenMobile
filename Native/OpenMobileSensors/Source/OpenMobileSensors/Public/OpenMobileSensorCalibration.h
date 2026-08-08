#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorAccuracy.h"
#include "OpenMobileSensorCalibration.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorCalibrationState : uint8
{
	Required UMETA(DisplayName = "Calibration Required", ToolTip = "The sensor needs user or platform calibration before reliable readings are expected."),
	Resolved UMETA(DisplayName = "Calibration Resolved", ToolTip = "The sensor has recovered to an acceptable calibration state.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorCalibrationReason : uint8
{
	NativeRequirement UMETA(DisplayName = "Platform Requested Calibration", ToolTip = "The native sensor provider requested its platform calibration workflow."),
	MagneticInterference UMETA(DisplayName = "Magnetic Interference", ToolTip = "Nearby magnetic interference reduced heading or magnetometer reliability."),
	QualityRecovered UMETA(DisplayName = "Quality Recovered", ToolTip = "Sensor quality recovered enough to clear the calibration request.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorCalibrationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorCalibrationState State =
		EOpenMobileSensorCalibrationState::Required;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorCalibrationReason Reason =
		EOpenMobileSensorCalibrationReason::NativeRequirement;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FText Guidance;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 Sequence = 0;
};

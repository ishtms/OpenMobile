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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor type and provider instance this value describes."))
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Current lifecycle state reported for this value."))
	EOpenMobileSensorCalibrationState State =
		EOpenMobileSensorCalibrationState::Required;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Reason for this sensor calibration event."))
	EOpenMobileSensorCalibrationReason Reason =
		EOpenMobileSensorCalibrationReason::NativeRequirement;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Provider-reported accuracy classification for this value."))
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Guidance for this sensor calibration event."))
	FText Guidance;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonic sensor-service timestamp in seconds for this value."))
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonically increasing sequence used to order values from this source."))
	int64 Sequence = 0;
};

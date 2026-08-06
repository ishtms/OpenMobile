#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorAccuracy.h"
#include "OpenMobileSensorCalibration.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorCalibrationState : uint8
{
	Required,
	Resolved
};

UENUM(BlueprintType)
enum class EOpenMobileSensorCalibrationReason : uint8
{
	NativeRequirement,
	MagneticInterference,
	QualityRecovered
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

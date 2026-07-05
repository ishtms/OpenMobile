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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorCalibrationState State =
		EOpenMobileSensorCalibrationState::Required;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorCalibrationReason Reason =
		EOpenMobileSensorCalibrationReason::NativeRequirement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FText Guidance;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 Sequence = 0;
};

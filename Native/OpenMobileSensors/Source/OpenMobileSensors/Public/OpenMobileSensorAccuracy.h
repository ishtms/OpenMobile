#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorAccuracy.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorAccuracy : uint8
{
	Unknown UMETA(DisplayName = "Unknown", ToolTip = "The provider did not report an accuracy estimate."),
	Unreliable UMETA(DisplayName = "Unreliable", ToolTip = "The reading should not be used until sensor accuracy recovers."),
	Low UMETA(DisplayName = "Low Accuracy", ToolTip = "The reading has low estimated accuracy and may require calibration."),
	Medium UMETA(DisplayName = "Medium Accuracy", ToolTip = "The reading has moderate estimated accuracy."),
	High UMETA(DisplayName = "High Accuracy", ToolTip = "The reading has the provider's highest reported accuracy level.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorAccuracySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasEstimatedError = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double EstimatedError = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 Sequence = 0;
};

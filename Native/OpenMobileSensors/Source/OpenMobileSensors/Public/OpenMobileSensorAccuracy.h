#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorAccuracy.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorAccuracy : uint8
{
	Unknown,
	Unreliable,
	Low,
	Medium,
	High
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

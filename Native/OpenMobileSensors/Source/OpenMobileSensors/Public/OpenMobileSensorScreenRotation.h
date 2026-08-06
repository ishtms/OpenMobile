#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorScreenRotation.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorScreenRotation : uint8
{
	Rotation0,
	Rotation90,
	Rotation180,
	Rotation270
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorScreenRotationSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorScreenRotation Rotation =
		EOpenMobileSensorScreenRotation::Rotation0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bNaturalOrientationLandscape = false;
};

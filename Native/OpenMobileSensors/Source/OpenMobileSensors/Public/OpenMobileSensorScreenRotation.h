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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorScreenRotation Rotation =
		EOpenMobileSensorScreenRotation::Rotation0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bNaturalOrientationLandscape = false;
};

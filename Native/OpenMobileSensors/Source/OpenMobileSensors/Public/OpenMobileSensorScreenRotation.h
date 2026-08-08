#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorScreenRotation.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorScreenRotation : uint8
{
	Rotation0 UMETA(DisplayName = "0 Degrees", ToolTip = "Uses the display's natural orientation without an additional clockwise rotation."),
	Rotation90 UMETA(DisplayName = "90 Degrees Clockwise", ToolTip = "Rotates device-fixed sensor axes 90 degrees clockwise into the current screen orientation."),
	Rotation180 UMETA(DisplayName = "180 Degrees", ToolTip = "Rotates device-fixed sensor axes 180 degrees into the current screen orientation."),
	Rotation270 UMETA(DisplayName = "270 Degrees Clockwise", ToolTip = "Rotates device-fixed sensor axes 270 degrees clockwise into the current screen orientation.")
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

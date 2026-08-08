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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Rotation for this sensor screen rotation snapshot."))
	EOpenMobileSensorScreenRotation Rotation =
		EOpenMobileSensorScreenRotation::Rotation0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonic sensor-service timestamp in seconds for this value."))
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonically increasing sequence used to order values from this source."))
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Coordinates", meta = (ToolTip = "Whether the device reports landscape as its natural display orientation."))
	bool bNaturalOrientationLandscape = false;
};

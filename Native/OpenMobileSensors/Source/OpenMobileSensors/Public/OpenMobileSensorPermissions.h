#pragma once

#include "CoreMinimal.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorPermissions.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorPermission : uint8
{
	MotionActivity,
	ActivityRecognition,
	TrueHeadingLocation
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorPermissionDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorPermission Permission =
		EOpenMobileSensorPermission::MotionActivity;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FName PermissionName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobilePermissionStatus Status =
		EOpenMobilePermissionStatus::NotDetermined;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorLocationInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ToolTip = "Geodetic latitude in degrees."))
	double LatitudeDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ToolTip = "Geodetic longitude in degrees."))
	double LongitudeDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ToolTip = "Altitude above the WGS84 ellipsoid in metres."))
	double AltitudeMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ToolTip = "Horizontal position accuracy radius in metres."))
	double HorizontalAccuracyMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ToolTip = "Location capture timestamp in Unix time seconds."))
	double TimestampSeconds = 0.0;
};

class OPENMOBILESENSORS_API FOpenMobileSensorPermissions final
{
public:
	static FName GetPermissionName(EOpenMobileSensorPermission Permission);
	static FString GetExplanation(EOpenMobileSensorPermission Permission);
	static FOpenMobileSensorPermissionDescriptor Describe(
		EOpenMobileSensorPermission Permission,
		EOpenMobilePermissionStatus Status =
			EOpenMobilePermissionStatus::NotDetermined
	);
};

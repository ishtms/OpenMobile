#pragma once

#include "CoreMinimal.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorPermissions.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorPermission : uint8
{
	MotionActivity,
	ActivityRecognition,
	TrueHeadingLocation UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EOpenMobileTrueHeadingLocationOutcome : uint8
{
	LocationAccepted UMETA(DisplayName = "Location Accepted", ToolTip = "The fresh authorized location fix was accepted for true-heading calculations."),
	PermissionMissing UMETA(DisplayName = "Permission Missing", ToolTip = "The external location provider has not granted usable location permission."),
	LocationStale UMETA(DisplayName = "Location Stale", ToolTip = "The location fix is older than the accepted true-heading freshness limit."),
	AccuracyTooLow UMETA(DisplayName = "Accuracy Too Low", ToolTip = "The location fix has a horizontal accuracy radius above the accepted limit."),
	InvalidInput UMETA(DisplayName = "Invalid Input", ToolTip = "One or more location fields are invalid for true-heading calculations.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorPermissionDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorPermission Permission =
		EOpenMobileSensorPermission::MotionActivity;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName PermissionName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobilePermissionStatus Status =
		EOpenMobilePermissionStatus::NotDetermined;
};

USTRUCT(BlueprintType, meta = (DisplayName = "True Heading Location Input (Advanced)"))
struct OPENMOBILESENSORS_API FOpenMobileSensorLocationInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Geodetic latitude in degrees."))
	double LatitudeDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Geodetic longitude in degrees."))
	double LongitudeDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Altitude above the WGS84 ellipsoid in metres."))
	double AltitudeMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Horizontal position accuracy radius in metres."))
	double HorizontalAccuracyMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Location capture timestamp in Unix time seconds."))
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

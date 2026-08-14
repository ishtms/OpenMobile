#pragma once

#include "CoreMinimal.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorPermissions.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorPermission : uint8
{
	MotionActivity UMETA(DisplayName = "Motion Activity (iOS)", ToolTip = "Allows access to Core Motion activity and pedometer data on Apple platforms."),
	ActivityRecognition UMETA(DisplayName = "Activity Recognition (Android)", ToolTip = "Allows access to activity recognition, step, and related motion data on Android."),
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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Permission for this sensor permission descriptor."))
	EOpenMobileSensorPermission Permission =
		EOpenMobileSensorPermission::MotionActivity;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Permission Name for this sensor permission descriptor."))
	FName PermissionName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Explanation for this sensor permission descriptor."))
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Current status reported for this value."))
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
	/** You'll get the stable platform-facing name for this sensor permission. UI text belongs in GetExplanation instead. */
	static FName GetPermissionName(EOpenMobileSensorPermission Permission);

	/** You'll get a short explanation users can actually act on. It doesn't request anything. */
	static FString GetExplanation(EOpenMobileSensorPermission Permission);

	/** Use this when code needs the permission name, explanation, and current status together. No platform prompt is opened here. */
	static FOpenMobileSensorPermissionDescriptor Describe(
		EOpenMobileSensorPermission Permission,
		EOpenMobilePermissionStatus Status =
			EOpenMobilePermissionStatus::NotDetermined
	);
};

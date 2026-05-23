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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double LatitudeDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double LongitudeDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double AltitudeMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double HorizontalAccuracyMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
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

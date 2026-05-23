#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorCapabilities.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorAvailabilitySource : uint8
{
	Unknown,
	Native,
	Derived,
	Mock,
	Replay
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRestriction : uint8
{
	None,
	Permission,
	RateLimited,
	Background,
	Configuration,
	MissingInput,
	Calibration,
	TemporarilyUnavailable
};

UENUM(BlueprintType)
enum class EOpenMobileSensorBackgroundSupport : uint8
{
	Unknown,
	Unsupported,
	Suspended,
	Limited,
	EventDriven,
	Supported
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileCapability Availability;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorAvailabilitySource Source =
		EOpenMobileSensorAvailabilitySource::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FName RequiredPermission;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorRestriction ActiveRestriction =
		EOpenMobileSensorRestriction::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double MinimumFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double MaximumFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bSupportsNativeBatching = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorBackgroundSupport BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::Unknown;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorCapabilitySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileCapability BackendAvailability;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 BackendGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileSensorCapability> Sensors;
};

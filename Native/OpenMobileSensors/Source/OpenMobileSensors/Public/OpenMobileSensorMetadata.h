#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorMetadata.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionalNumber
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double Value = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionalInteger
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 Value = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionalText
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FString Value;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionalBoolean
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bValue = false;
};

UENUM(BlueprintType)
enum class EOpenMobileSensorReportingMode : uint8
{
	Unknown,
	Continuous,
	OnChange,
	OneShot,
	SpecialTrigger
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bPreferred = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalText Vendor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalText NativeName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalInteger Version;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalNumber MaximumRange;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalNumber Resolution;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalNumber EstimatedPowerMilliwatts;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalNumber MinimumIntervalSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalNumber MaximumIntervalSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalInteger FifoCapacitySamples;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOptionalBoolean WakeUpBehavior;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bReportingModeAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorReportingMode ReportingMode =
		EOpenMobileSensorReportingMode::Unknown;
};

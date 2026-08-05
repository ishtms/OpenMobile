#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorMetadata.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Optional Sensor Number", ToolTip = "A sensor number that may not be reported by the active platform provider."))
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionalNumber
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Values", meta = (ToolTip = "Whether Value is available."))
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Values", meta = (AdvancedDisplay, ToolTip = "Raw backing number. Read only when Available is true, or use an optional-value helper."))
	double Value = 0.0;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Optional Sensor Integer", ToolTip = "A sensor integer that may not be reported by the active platform provider."))
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionalInteger
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Values", meta = (ToolTip = "Whether Value is available."))
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Values", meta = (AdvancedDisplay, ToolTip = "Raw backing integer. Read only when Available is true, or use an optional-value helper."))
	int64 Value = 0;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Optional Sensor Text", ToolTip = "Sensor text that may not be reported by the active platform provider."))
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionalText
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Values", meta = (ToolTip = "Whether Value is available."))
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Values", meta = (AdvancedDisplay, ToolTip = "Raw backing text. Read only when Available is true, or use an optional-value helper."))
	FString Value;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Optional Sensor Boolean", ToolTip = "A sensor Boolean that may not be reported by the active platform provider."))
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionalBoolean
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Values", meta = (ToolTip = "Whether Value is available."))
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Values", meta = (AdvancedDisplay, ToolTip = "Raw backing Boolean. Read only when Available is true, or use an optional-value helper."))
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

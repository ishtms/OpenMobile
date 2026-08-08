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
	Unknown UMETA(DisplayName = "Unknown", ToolTip = "The platform did not report a sensor reporting mode."),
	Continuous UMETA(DisplayName = "Continuous", ToolTip = "The sensor produces samples continuously at its resolved rate."),
	OnChange UMETA(DisplayName = "On Change", ToolTip = "The sensor produces a sample when its measured state changes."),
	OneShot UMETA(DisplayName = "One Shot", ToolTip = "The sensor produces one sample and then stops."),
	SpecialTrigger UMETA(DisplayName = "Special Trigger", ToolTip = "The sensor reports discrete trigger events using platform-specific timing.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor type and provider instance this value describes."))
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Whether this is the provider's preferred instance when more than one sensor of the same type exists."))
	bool bPreferred = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Vendor for this sensor metadata."))
	FOpenMobileSensorOptionalText Vendor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Native Name for this sensor metadata."))
	FOpenMobileSensorOptionalText NativeName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Version for this sensor metadata."))
	FOpenMobileSensorOptionalInteger Version;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Maximum measurable magnitude in the sensor type's standardized units, when reported by the platform."))
	FOpenMobileSensorOptionalNumber MaximumRange;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Smallest reported measurement increment in the sensor type's standardized units, when available."))
	FOpenMobileSensorOptionalNumber Resolution;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Platform-estimated sensor power draw in milliwatts, when available."))
	FOpenMobileSensorOptionalNumber EstimatedPowerMilliwatts;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Shortest supported sample interval in seconds, equivalent to the sensor's highest native rate."))
	FOpenMobileSensorOptionalNumber MinimumIntervalSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Longest supported sample interval in seconds, when the platform reports a minimum native rate."))
	FOpenMobileSensorOptionalNumber MaximumIntervalSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Maximum native hardware FIFO capacity in samples, when reported by the platform."))
	FOpenMobileSensorOptionalInteger FifoCapacitySamples;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Whether the native sensor can wake the application processor to deliver qualifying events, when reported."))
	FOpenMobileSensorOptionalBoolean WakeUpBehavior;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Whether Reporting Mode was explicitly reported by the active platform provider."))
	bool bReportingModeAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Discovery|Metadata", meta = (ToolTip = "Native continuous, on-change, one-shot, or trigger delivery behavior. Read only when Reporting Mode Available is true."))
	EOpenMobileSensorReportingMode ReportingMode =
		EOpenMobileSensorReportingMode::Unknown;
};

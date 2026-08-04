#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorMetadata.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorDiscoveryLibrary.generated.h"

class UOpenMobileSensorsSubsystem;

UENUM(BlueprintType)
enum class EOpenMobileSensorAccessRequirement : uint8
{
	None UMETA(DisplayName = "No Permission Needed", ToolTip = "This sensor can start without a user permission prompt."),
	SensorPermission UMETA(DisplayName = "Sensor Permission", ToolTip = "This sensor requires a permission owned by OpenMobile Sensors."),
	ExternalPrerequisite UMETA(DisplayName = "External Prerequisite", ToolTip = "This sensor requires data or permission owned by another provider, such as location.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorAvailabilityBranch : uint8
{
	Available UMETA(DisplayName = "Available", ToolTip = "The selected sensor can start now."),
	PermissionRequired UMETA(DisplayName = "Permission Required", ToolTip = "The selected sensor needs a user permission before it can start."),
	TemporarilyUnavailable UMETA(DisplayName = "Temporarily Unavailable", ToolTip = "The selected sensor may become available after lifecycle or backend recovery."),
	Unavailable UMETA(DisplayName = "Unavailable", ToolTip = "The selected sensor cannot start in its current configuration.")
};

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Availability Info"))
struct OPENMOBILESENSORS_API FOpenMobileSensorAvailabilityInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Resolved sensor instance, or the requested sensor type when no matching instance was discovered."))
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Whether this sensor can start now."))
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Current availability state."))
	EOpenMobileCapabilityState State = EOpenMobileCapabilityState::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Provider supplying this sensor."))
	EOpenMobileSensorAvailabilitySource Source =
		EOpenMobileSensorAvailabilitySource::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Active reason this sensor cannot start."))
	EOpenMobileSensorRestriction Restriction =
		EOpenMobileSensorRestriction::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Permission that must be granted before this sensor can start, when applicable."))
	FName RequiredPermission;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Lowest rate reported by the selected sensor, in hertz."))
	double MinimumFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Highest rate reported by the selected sensor, in hertz."))
	double MaximumFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Provider detail explaining the current state."))
	FText Detail;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Recommended user or developer action for the current state."))
	FText RequiredAction;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Display Info"))
struct OPENMOBILESENSORS_API FOpenMobileSensorDisplayInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Human-readable sensor name for UI."))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Short public unit used by the preferred listener output."))
	FText Unit;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Short explanation of the sensor's normal value."))
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Generic sample family used only by advanced raw graphs."))
	EOpenMobileSensorSampleFamily SampleFamily =
		EOpenMobileSensorSampleFamily::Unknown;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Access Requirement"))
struct OPENMOBILESENSORS_API FOpenMobileSensorAccessRequirement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Kind of access required before this sensor can start."))
	EOpenMobileSensorAccessRequirement Requirement =
		EOpenMobileSensorAccessRequirement::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Typed sensor permission when Requirement is Sensor Permission."))
	EOpenMobileSensorPermission Permission =
		EOpenMobileSensorPermission::MotionActivity;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Stable access or prerequisite name."))
	FName AccessName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "User-facing explanation of why access is needed."))
	FText Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Immediate action that can satisfy the requirement."))
	FText Correction;
};

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorDiscoveryLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (WorldContext = "WorldContextObject", DisplayName = "Get OpenMobile Sensors", Keywords = "OpenMobile mobile sensors subsystem service accelerometer gyroscope gyro motion activity compass heading pressure barometer steps", ToolTip = "Returns the OpenMobile Sensors Game Instance subsystem for this world."))
	static UOpenMobileSensorsSubsystem* GetOpenMobileSensorsSubsystem(
		const UObject* WorldContextObject
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (WorldContext = "WorldContextObject", DisplayName = "Is Sensor Available", Keywords = "OpenMobile sensors available supported ready hardware", ToolTip = "Returns whether the preferred instance of this sensor is currently available."))
	static bool IsSensorAvailable(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Sensor Availability", Keywords = "OpenMobile sensors capability restriction correction", ToolTip = "Returns the preferred capability for one sensor without an array search."))
	static bool GetSensorAvailability(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor,
		FOpenMobileSensorCapability& OutCapability,
		FName InstanceId = NAME_None
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Sensor Availability Info", Keywords = "OpenMobile sensors compact capability restriction correction action rate", AdvancedDisplay = "InstanceId", ToolTip = "Returns compact availability and correction data for the preferred or named sensor instance."))
	static bool GetSensorAvailabilityInfo(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor,
		FOpenMobileSensorAvailabilityInfo& OutAvailability,
		FName InstanceId = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (WorldContext = "WorldContextObject", DisplayName = "Branch on Sensor Availability", ExpandEnumAsExecs = "Branch", Keywords = "OpenMobile sensors available permission required unavailable temporary branch", AdvancedDisplay = "InstanceId", ToolTip = "Routes the current preferred or named sensor state and returns a compact explanation and correction."))
	static void BranchSensorAvailability(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor,
		FOpenMobileSensorAvailabilityInfo& OutAvailability,
		EOpenMobileSensorAvailabilityBranch& Branch,
		FName InstanceId = NAME_None
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Available Sensors", Keywords = "OpenMobile sensors list enumerate available", ToolTip = "Returns every currently available sensor instance."))
	static TArray<FOpenMobileSensorIdentifier> GetAvailableSensors(
		const UObject* WorldContextObject
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Preferred Sensor", Keywords = "OpenMobile sensors preferred default instance", ToolTip = "Returns the preferred discovered instance for a sensor type."))
	static bool GetPreferredSensor(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor,
		FOpenMobileSensorIdentifier& OutSensor
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Sensor Metadata", Keywords = "OpenMobile sensors metadata vendor range resolution power", ToolTip = "Returns metadata for the preferred sensor instance without an array search."))
	static bool GetSensorMetadata(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor,
		FOpenMobileSensorMetadata& OutMetadata
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Sensor Display Info", Keywords = "OpenMobile sensors display name unit description family", ToolTip = "Returns the public name, unit, description, and advanced sample family for a sensor."))
	static FOpenMobileSensorDisplayInfo GetSensorDisplayInfo(
		EOpenMobileSensorType Sensor
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Permissions", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Required Access for Sensor", Keywords = "OpenMobile sensors permission prerequisite access location", ToolTip = "Returns a typed permission or external prerequisite for the preferred sensor instance."))
	static FOpenMobileSensorAccessRequirement GetRequiredAccessForSensor(
		const UObject* WorldContextObject,
		EOpenMobileSensorType Sensor
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Permissions", meta = (DisplayName = "Describe Sensor Permission", Keywords = "OpenMobile sensors permission name explanation", ToolTip = "Returns the stable name and platform explanation for a requestable sensor permission."))
	static FOpenMobileSensorPermissionDescriptor DescribeSensorPermission(
		EOpenMobileSensorPermission Permission,
		EOpenMobilePermissionStatus Status =
			EOpenMobilePermissionStatus::NotDetermined
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Configured Sensor Rate Preset", Keywords = "OpenMobile sensors rate preset configured frequency latency callback power", ToolTip = "Returns the current Project Settings values for a rate preset."))
	static FOpenMobileSensorRatePresetSettings GetConfiguredSensorRatePreset(
		EOpenMobileSensorRatePreset Preset
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Recommended Sensor Options", Keywords = "OpenMobile sensors recommended options defaults event", ToolTip = "Returns Project Settings defaults resolved for an event-driven preferred listener."))
	static FOpenMobileSensorStreamOptions GetRecommendedSensorOptions(
		EOpenMobileSensorType Sensor
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Preview Sensor Stream Options", Keywords = "OpenMobile sensors preview resolve rate clamp options", ToolTip = "Resolves presets, sensor limits, and project policy without starting hardware."))
	static bool PreviewSensorStreamOptions(
		EOpenMobileSensorType Sensor,
		const FOpenMobileSensorStreamOptions& RequestedOptions,
		FOpenMobileSensorStreamOptions& OutAppliedOptions,
		FOpenMobileSensorRateResolution& OutRateResolution
	);
};

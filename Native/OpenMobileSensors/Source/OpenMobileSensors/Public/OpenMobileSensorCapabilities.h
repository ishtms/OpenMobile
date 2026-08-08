#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorErrors.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorQuality.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorCapabilities.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorAvailabilitySource : uint8
{
	Unknown UMETA(DisplayName = "Unknown Source", ToolTip = "No active provider source has been identified for this sensor."),
	Native UMETA(DisplayName = "Native Sensor", ToolTip = "The platform exposes this sensor directly through a native API."),
	Derived UMETA(DisplayName = "Plugin Derived", ToolTip = "OpenMobile Sensors derives this value from one or more input sensors."),
	Mock UMETA(DisplayName = "Development Mock", ToolTip = "A development mock provider supplies this sensor."),
	Replay UMETA(DisplayName = "Recording Replay", ToolTip = "A recorded stream supplies this sensor.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRestriction : uint8
{
	None UMETA(DisplayName = "No Restriction", ToolTip = "No known restriction prevents the requested sensor operation."),
	MissingHardware UMETA(DisplayName = "Missing Hardware", ToolTip = "The device does not expose the hardware needed for this sensor."),
	Permission UMETA(DisplayName = "Permission Required", ToolTip = "A required user permission is missing, denied, or restricted."),
	RateLimited UMETA(DisplayName = "Rate Limited", ToolTip = "Platform or project policy limits the requested sampling rate."),
	Background UMETA(DisplayName = "Background Restricted", ToolTip = "The operation cannot continue with the application's current lifecycle state or policy."),
	Configuration UMETA(DisplayName = "Configuration Blocked", ToolTip = "Project or platform configuration prevents this sensor operation."),
	MissingInput UMETA(DisplayName = "Derived Input Missing", ToolTip = "A sensor needed to derive this value is unavailable."),
	Calibration UMETA(DisplayName = "Calibration Required", ToolTip = "The sensor needs calibration before reliable operation can continue."),
	TemporarilyUnavailable UMETA(DisplayName = "Temporarily Unavailable", ToolTip = "The provider may recover after a lifecycle, service, or hardware state change.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorBackgroundSupport : uint8
{
	Unknown UMETA(DisplayName = "Unknown", ToolTip = "Background behavior is not known for this platform and operation."),
	Unsupported UMETA(DisplayName = "Unsupported", ToolTip = "The platform cannot perform this sensor operation in the background."),
	Suspended UMETA(DisplayName = "Suspended", ToolTip = "The operation pauses in the background and may resume when the app becomes active."),
	Limited UMETA(DisplayName = "Limited", ToolTip = "Background operation is available with platform-specific rate or duration limits."),
	EventDriven UMETA(DisplayName = "Event Driven", ToolTip = "The platform can deliver only qualifying background events, not a continuous stream."),
	Supported UMETA(DisplayName = "Supported", ToolTip = "The platform supports this operation in the background when project policy allows it.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorBackgroundOperation : uint8
{
	Stream UMETA(DisplayName = "Live Stream", ToolTip = "Checks background support for an active live sensor stream."),
	Recording UMETA(DisplayName = "Recording", ToolTip = "Checks background support for recording sensor samples."),
	NativeStepCountQuery UMETA(DisplayName = "Native Step Count Query", ToolTip = "Checks whether the platform can answer a native historical step count query while backgrounded.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorFallbackPowerCost : uint8
{
	Unknown UMETA(DisplayName = "Unknown Cost", ToolTip = "The fallback's expected power cost has not been characterized."),
	Low UMETA(DisplayName = "Low Cost", ToolTip = "The fallback is expected to add little sensor or CPU power use."),
	Moderate UMETA(DisplayName = "Moderate Cost", ToolTip = "The fallback requires additional sensing or processing with a noticeable power cost."),
	High UMETA(DisplayName = "High Cost", ToolTip = "The fallback requires sustained sensing or processing with a significant power cost.")
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileSensorFallbackUnsupportedCondition : uint8
{
	None = 0 UMETA(DisplayName = "No Unsupported Condition", ToolTip = "No known condition prevents use of the derived fallback."),
	MissingInput = 1 << 0 UMETA(DisplayName = "Input Missing", ToolTip = "One or more sensors required by the fallback are unavailable."),
	InsufficientRate = 1 << 1 UMETA(DisplayName = "Input Rate Too Low", ToolTip = "An input sensor cannot meet the fallback's minimum frequency."),
	UncalibratedInput = 1 << 2 UMETA(DisplayName = "Input Not Calibrated", ToolTip = "The fallback requires calibrated input that is not currently available."),
	PermissionUnavailable = 1 << 3 UMETA(DisplayName = "Permission Unavailable", ToolTip = "A permission required by the fallback is missing, denied, or restricted."),
	LifecycleUnavailable = 1 << 4 UMETA(DisplayName = "Lifecycle Unavailable", ToolTip = "The current foreground or background state prevents the fallback from running.")
};
ENUM_CLASS_FLAGS(EOpenMobileSensorFallbackUnsupportedCondition);

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorPrerequisiteCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileCapability Availability;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFailureReason InputFailureReason =
		EOpenMobileSensorFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName RequiredPermission;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bPermissionStatusKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobilePermissionStatus PermissionStatus =
		EOpenMobilePermissionStatus::NotDetermined;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFailureReason PermissionFailureReason =
		EOpenMobileSensorFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double MaximumAgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double MaximumHorizontalAccuracyMeters = 0.0;

	bool operator==(
		const FOpenMobileSensorPrerequisiteCapability& Other
	) const
	{
		return Name == Other.Name
			&& Availability.Name == Other.Availability.Name
			&& Availability.State == Other.Availability.State
			&& Availability.Detail == Other.Availability.Detail
			&& InputFailureReason == Other.InputFailureReason
			&& RequiredPermission == Other.RequiredPermission
			&& bPermissionStatusKnown == Other.bPermissionStatusKnown
			&& PermissionStatus == Other.PermissionStatus
			&& PermissionFailureReason == Other.PermissionFailureReason
			&& bReady == Other.bReady
			&& MaximumAgeSeconds == Other.MaximumAgeSeconds
			&& MaximumHorizontalAccuracyMeters ==
				Other.MaximumHorizontalAccuracyMeters;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorFallbackCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bImplemented = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bRequiredInputsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<EOpenMobileSensorType> RequiredInputs;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double MinimumInputFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bRequiresCalibratedInput = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFusionQuality ExpectedQuality =
		EOpenMobileSensorFusionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFallbackPowerCost PowerCost =
		EOpenMobileSensorFallbackPowerCost::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double CpuBudgetMicrosecondsPerSample = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorFallbackUnsupportedCondition"))
	int32 UnsupportedConditionFlags = 0;

	bool operator==(
		const FOpenMobileSensorFallbackCapability& Other
	) const
	{
		return bImplemented == Other.bImplemented
			&& bAvailable == Other.bAvailable
			&& bRequiredInputsAvailable == Other.bRequiredInputsAvailable
			&& RequiredInputs == Other.RequiredInputs
			&& MinimumInputFrequencyHz == Other.MinimumInputFrequencyHz
			&& bRequiresCalibratedInput == Other.bRequiresCalibratedInput
			&& ExpectedQuality == Other.ExpectedQuality
			&& PowerCost == Other.PowerCost
			&& CpuBudgetMicrosecondsPerSample ==
				Other.CpuBudgetMicrosecondsPerSample
			&& UnsupportedConditionFlags ==
				Other.UnsupportedConditionFlags;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeReferenceFrameCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileAttitudeReferenceFrame ReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileCapability Availability;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bMayUseFallback = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHeadingDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bLocationDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bExpectedToDrift = false;

	bool operator==(
		const FOpenMobileAttitudeReferenceFrameCapability& Other
	) const
	{
		return ReferenceFrame == Other.ReferenceFrame
			&& Availability.Name == Other.Availability.Name
			&& Availability.State == Other.Availability.State
			&& Availability.Detail == Other.Availability.Detail
			&& bMayUseFallback == Other.bMayUseFallback
			&& bHeadingDependent == Other.bHeadingDependent
			&& bLocationDependent == Other.bLocationDependent
			&& bCalibrationRequired == Other.bCalibrationRequired
			&& bExpectedToDrift == Other.bExpectedToDrift;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorBackgroundCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorBackgroundOperation Operation =
		EOpenMobileSensorBackgroundOperation::Stream;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorBackgroundSupport PlatformBehavior =
		EOpenMobileSensorBackgroundSupport::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorBackgroundSupport ExpectedBehavior =
		EOpenMobileSensorBackgroundSupport::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorRestriction ActiveRestriction =
		EOpenMobileSensorRestriction::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFailureReason Reason =
		EOpenMobileSensorFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName RequiredPermission;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bProjectOptInRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bProjectOptInEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FString Detail;

	bool operator==(
		const FOpenMobileSensorBackgroundCapability& Other
	) const
	{
		return Operation == Other.Operation
			&& Sensor == Other.Sensor
			&& PlatformBehavior == Other.PlatformBehavior
			&& ExpectedBehavior == Other.ExpectedBehavior
			&& ActiveRestriction == Other.ActiveRestriction
			&& Reason == Other.Reason
			&& RequiredPermission == Other.RequiredPermission
			&& bProjectOptInRequired == Other.bProjectOptInRequired
			&& bProjectOptInEnabled == Other.bProjectOptInEnabled
			&& Detail == Other.Detail;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileCapability Availability;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorAvailabilitySource Source =
		EOpenMobileSensorAvailabilitySource::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName RequiredPermission;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorRestriction ActiveRestriction =
		EOpenMobileSensorRestriction::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double MinimumFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double MaximumFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bSupportsNativeBatching = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorBackgroundSupport BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorBackgroundCapability> BackgroundOperations;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorFallbackCapability Fallback;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorPrerequisiteCapability> Prerequisites;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileAttitudeReferenceFrameCapability>
		AttitudeReferenceFrames;

	bool operator==(const FOpenMobileSensorCapability& Other) const
	{
		return Sensor == Other.Sensor
			&& Availability.Name == Other.Availability.Name
			&& Availability.State == Other.Availability.State
			&& Availability.Detail == Other.Availability.Detail
			&& Source == Other.Source
			&& RequiredPermission == Other.RequiredPermission
			&& ActiveRestriction == Other.ActiveRestriction
			&& MinimumFrequencyHz == Other.MinimumFrequencyHz
			&& MaximumFrequencyHz == Other.MaximumFrequencyHz
			&& bSupportsNativeBatching == Other.bSupportsNativeBatching
			&& BackgroundSupport == Other.BackgroundSupport
			&& BackgroundOperations == Other.BackgroundOperations
			&& Fallback == Other.Fallback
			&& Prerequisites == Other.Prerequisites
			&& AttitudeReferenceFrames == Other.AttitudeReferenceFrames;
	}

	bool operator!=(const FOpenMobileSensorCapability& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorCapabilitySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileCapability BackendAvailability;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 BackendGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileSensorCapability> Sensors;

	bool operator==(const FOpenMobileSensorCapabilitySnapshot& Other) const
	{
		return BackendName == Other.BackendName
			&& BackendAvailability.Name == Other.BackendAvailability.Name
			&& BackendAvailability.State == Other.BackendAvailability.State
			&& BackendAvailability.Detail == Other.BackendAvailability.Detail
			&& BackendGeneration == Other.BackendGeneration
			&& Sensors == Other.Sensors;
	}

	bool operator!=(const FOpenMobileSensorCapabilitySnapshot& Other) const
	{
		return !(*this == Other);
	}
};

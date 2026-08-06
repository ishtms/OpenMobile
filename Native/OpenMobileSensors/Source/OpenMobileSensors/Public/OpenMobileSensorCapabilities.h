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
	MissingHardware,
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

UENUM(BlueprintType)
enum class EOpenMobileSensorBackgroundOperation : uint8
{
	Stream,
	Recording,
	NativeStepCountQuery
};

UENUM(BlueprintType)
enum class EOpenMobileSensorFallbackPowerCost : uint8
{
	Unknown,
	Low,
	Moderate,
	High
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileSensorFallbackUnsupportedCondition : uint8
{
	None = 0,
	MissingInput = 1 << 0,
	InsufficientRate = 1 << 1,
	UncalibratedInput = 1 << 2,
	PermissionUnavailable = 1 << 3,
	LifecycleUnavailable = 1 << 4
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

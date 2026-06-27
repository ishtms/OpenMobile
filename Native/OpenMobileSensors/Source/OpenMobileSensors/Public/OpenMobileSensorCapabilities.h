#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorIdentifiers.h"
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

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeReferenceFrameCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileAttitudeReferenceFrame ReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileCapability Availability;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bMayUseFallback = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHeadingDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bLocationDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileCapability BackendAvailability;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 BackendGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
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

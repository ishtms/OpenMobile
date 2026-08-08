#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorErrors.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorSubscription.generated.h"

class UOpenMobileSensorsSubsystem;
class FOpenMobileSensorsSubscriptionService;

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorSubscriptionHandle
{
	GENERATED_BODY()

	bool IsValid() const
	{
		return Identifier.IsValid() && Generation != 0;
	}

	void Reset()
	{
		Identifier.Invalidate();
		Generation = 0;
	}

	bool operator==(const FOpenMobileSensorSubscriptionHandle& Other) const
	{
		return Identifier == Other.Identifier && Generation == Other.Generation;
	}

	bool operator!=(const FOpenMobileSensorSubscriptionHandle& Other) const
	{
		return !(*this == Other);
	}

	FGuid GetIdentifier() const
	{
		return Identifier;
	}

	uint32 GetGeneration() const
	{
		return Generation;
	}

	friend uint32 GetTypeHash(
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		return HashCombine(
			GetTypeHash(Handle.Identifier),
			GetTypeHash(Handle.Generation)
		);
	}

private:
	FGuid Identifier;
	uint32 Generation = 0;

	friend class UOpenMobileSensorsSubsystem;
	friend class FOpenMobileSensorsSubscriptionService;
};

UENUM(BlueprintType)
enum class EOpenMobileSensorSubscriptionState : uint8
{
	Invalid UMETA(DisplayName = "Invalid", ToolTip = "The subscription handle was never valid or no longer belongs to this Game Instance."),
	Accepted UMETA(DisplayName = "Accepted", ToolTip = "The request passed synchronous validation and is waiting for asynchronous startup."),
	Starting UMETA(DisplayName = "Starting", ToolTip = "The provider is starting the physical sensor stream."),
	Active UMETA(DisplayName = "Active", ToolTip = "The subscription is active and can receive samples."),
	Paused UMETA(DisplayName = "Paused", ToolTip = "Delivery is paused by lifecycle or explicit control and may resume."),
	Stopping UMETA(DisplayName = "Stopping", ToolTip = "A stop request is being completed by the provider."),
	Stopped UMETA(DisplayName = "Stopped", ToolTip = "The subscription reached a normal terminal state."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "The subscription reached a terminal failure state.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRecenterMode : uint8
{
	FullAttitude UMETA(DisplayName = "Full Attitude", ToolTip = "Makes the current attitude the new local orientation origin."),
	YawOnly UMETA(DisplayName = "Yaw Only", ToolTip = "Makes the current heading the new local yaw origin while preserving pitch and roll."),
	Clear UMETA(DisplayName = "Clear Recenter", ToolTip = "Removes the listener's recenter transform and returns to its provider reference frame.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRecenterState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorRecenterMode Mode =
		EOpenMobileSensorRecenterMode::FullAttitude;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FQuat InverseReference = FQuat::Identity;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeReferenceState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileAttitudeReferenceFrame RequestedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileAttitudeReferenceFrame AppliedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bFallbackApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHeadingDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bLocationDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bExpectedToDrift = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorSubscriptionStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bResettableStepCountSession = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorSubscriptionState State =
		EOpenMobileSensorSubscriptionState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorStreamOptions RequestedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorStreamOptions AppliedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorRateResolution RateResolution;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileAttitudeReferenceState AttitudeReference;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorRecenterState Recenter;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileError Error;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorFailureDetails Failure;
};

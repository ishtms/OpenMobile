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
	Invalid,
	Accepted,
	Starting,
	Active,
	Paused,
	Stopping,
	Stopped,
	Failed
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRecenterMode : uint8
{
	FullAttitude,
	YawOnly,
	Clear
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

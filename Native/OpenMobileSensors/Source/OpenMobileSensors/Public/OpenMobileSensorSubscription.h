#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
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

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeReferenceState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileAttitudeReferenceFrame RequestedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileAttitudeReferenceFrame AppliedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bFallbackApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHeadingDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bLocationDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bExpectedToDrift = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorSubscriptionStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorSubscriptionState State =
		EOpenMobileSensorSubscriptionState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorStreamOptions RequestedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorStreamOptions AppliedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorRateResolution RateResolution;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileAttitudeReferenceState AttitudeReference;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileError Error;
};

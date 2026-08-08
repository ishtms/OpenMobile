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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|State|Recenter", meta = (ToolTip = "Whether a recenter transform is currently applied to this subscription."))
	bool bApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Mode for this sensor recenter state."))
	EOpenMobileSensorRecenterMode Mode =
		EOpenMobileSensorRecenterMode::FullAttitude;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Inverse Reference for this sensor recenter state."))
	FQuat InverseReference = FQuat::Identity;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeReferenceState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Requested Reference Frame for this attitude reference state."))
	EOpenMobileAttitudeReferenceFrame RequestedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Applied Reference Frame for this attitude reference state."))
	EOpenMobileAttitudeReferenceFrame AppliedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|State|Attitude", meta = (ToolTip = "Whether the applied reference frame differs from the requested frame because a fallback was used."))
	bool bFallbackApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|State|Attitude", meta = (ToolTip = "Whether the applied reference frame depends on heading input."))
	bool bHeadingDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|State|Attitude", meta = (ToolTip = "Whether the applied reference frame depends on an authorized fresh location fix."))
	bool bLocationDependent = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|State|Attitude", meta = (ToolTip = "Whether the current provider reports that calibration is required for this reference frame."))
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|State|Attitude", meta = (ToolTip = "Whether yaw may drift because the applied frame has no magnetic or true-north anchor."))
	bool bExpectedToDrift = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorSubscriptionStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Typed identifier for the subscription or session this value describes."))
	FOpenMobileSensorSubscriptionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor type and provider instance this value describes."))
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|State", meta = (AdvancedDisplay, ToolTip = "Whether this Step Counter subscription reports an independently resettable count since its listener session began."))
	bool bResettableStepCountSession = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Current lifecycle state reported for this value."))
	EOpenMobileSensorSubscriptionState State =
		EOpenMobileSensorSubscriptionState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Stream options requested by the caller before validation and rate resolution."))
	FOpenMobileSensorStreamOptions RequestedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Validated stream options currently applied by the provider."))
	FOpenMobileSensorStreamOptions AppliedOptions;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Requested, clamped, and native rates with any adjustment explanation."))
	FOpenMobileSensorRateResolution RateResolution;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Attitude Reference for this sensor subscription state snapshot."))
	FOpenMobileAttitudeReferenceState AttitudeReference;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Recenter for this sensor subscription state snapshot."))
	FOpenMobileSensorRecenterState Recenter;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Stable error name and developer-facing diagnostic detail."))
	FOpenMobileError Error;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Structured sensor failure reason and native provider context."))
	FOpenMobileSensorFailureDetails Failure;
};

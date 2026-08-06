#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorErrors.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorFailureReason : uint8
{
	None,
	UnsupportedPlatform,
	UnsupportedOperation,
	MissingHardware,
	DerivedInputUnavailable,
	PermissionRequired,
	PermissionDenied,
	PermissionRestricted,
	RateLimited,
	InvalidRequest,
	InvalidHandle,
	StaleHandle,
	InvalidFrequency,
	InvalidReferenceFrame,
	PoorCalibration,
	BackgroundRestricted,
	BufferOverflow,
	MissingLocationInput,
	StaleLocationInput,
	PoorLocationAccuracy,
	TemporarilyUnavailable,
	ConfigurationBlocked,
	OperationalFailure,
	Cancelled,
	Internal
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorFailureDetails
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFailureReason Reason =
		EOpenMobileSensorFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FString NativeDomain;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FString NativeCode;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FString Correction;

	bool IsSet() const
	{
		return Reason != EOpenMobileSensorFailureReason::None;
	}
};

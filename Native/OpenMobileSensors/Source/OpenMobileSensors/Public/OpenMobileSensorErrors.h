#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorErrors.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorFailureReason : uint8
{
	None UMETA(DisplayName = "No Failure", ToolTip = "The operation did not report a failure."),
	UnsupportedPlatform UMETA(DisplayName = "Unsupported Platform", ToolTip = "OpenMobile Sensors does not support this operation on the current platform."),
	UnsupportedOperation UMETA(DisplayName = "Unsupported Operation", ToolTip = "The active provider does not implement the requested operation."),
	MissingHardware UMETA(DisplayName = "Missing Hardware", ToolTip = "The device does not expose the sensor hardware needed by the request."),
	DerivedInputUnavailable UMETA(DisplayName = "Derived Input Unavailable", ToolTip = "A sensor required to derive the requested value is unavailable."),
	PermissionRequired UMETA(DisplayName = "Permission Required", ToolTip = "A user permission must be requested before the operation can start."),
	PermissionDenied UMETA(DisplayName = "Permission Denied", ToolTip = "The user denied a permission required by the operation."),
	PermissionRestricted UMETA(DisplayName = "Permission Restricted", ToolTip = "System policy prevents use of a permission required by the operation."),
	RateLimited UMETA(DisplayName = "Rate Limited", ToolTip = "The requested sampling behavior exceeds a platform, project, or provider limit."),
	InvalidRequest UMETA(DisplayName = "Invalid Request", ToolTip = "One or more request fields are invalid or inconsistent."),
	InvalidHandle UMETA(DisplayName = "Invalid Handle", ToolTip = "The supplied subscription or session handle was never valid."),
	StaleHandle UMETA(DisplayName = "Stale Handle", ToolTip = "The supplied handle no longer belongs to an active operation or Game Instance."),
	InvalidFrequency UMETA(DisplayName = "Invalid Frequency", ToolTip = "The requested custom frequency is outside the accepted range."),
	InvalidReferenceFrame UMETA(DisplayName = "Invalid Reference Frame", ToolTip = "The requested coordinate or attitude frame is unsupported for this sensor."),
	PoorCalibration UMETA(DisplayName = "Poor Calibration", ToolTip = "Sensor calibration is insufficient for reliable operation."),
	BackgroundRestricted UMETA(DisplayName = "Background Restricted", ToolTip = "Lifecycle state or project policy prevents the requested background behavior."),
	BufferOverflow UMETA(DisplayName = "Buffer Overflow", ToolTip = "Samples were lost or rejected because a stream buffer reached capacity."),
	MissingLocationInput UMETA(DisplayName = "Location Input Missing", ToolTip = "True heading requires an authorized location fix that was not supplied."),
	StaleLocationInput UMETA(DisplayName = "Location Input Stale", ToolTip = "The supplied location fix is older than the accepted true-heading limit."),
	PoorLocationAccuracy UMETA(DisplayName = "Location Accuracy Too Low", ToolTip = "The supplied location accuracy radius exceeds the accepted true-heading limit."),
	TemporarilyUnavailable UMETA(DisplayName = "Temporarily Unavailable", ToolTip = "The provider may recover after a lifecycle, service, or hardware state change."),
	ConfigurationBlocked UMETA(DisplayName = "Configuration Blocked", ToolTip = "Project or platform configuration prevents the requested operation."),
	OperationalFailure UMETA(DisplayName = "Operational Failure", ToolTip = "The provider failed while performing an otherwise valid request."),
	Cancelled UMETA(DisplayName = "Cancelled", ToolTip = "The operation was cancelled before successful completion."),
	Internal UMETA(DisplayName = "Internal Error", ToolTip = "An unexpected internal sensor state prevented completion.")
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

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorAccuracy.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorStreamOptions.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorRatePreset : uint8
{
	UI UMETA(DisplayName = "UI, Low Power", ToolTip = "Uses the configurable UI preset for low-power interface motion. Read its current rate from OpenMobile Sensors Project Settings."),
	Game UMETA(DisplayName = "Game, Balanced", ToolTip = "Uses the configurable Game preset for balanced gameplay responsiveness and power use. Read its current rate from OpenMobile Sensors Project Settings."),
	Fast UMETA(DisplayName = "Fast, Performance", ToolTip = "Uses the configurable Fast preset for performance-sensitive motion. Read its current rate from OpenMobile Sensors Project Settings."),
	Custom UMETA(DisplayName = "Custom Rate", ToolTip = "Uses the request's explicit custom frequency, latency, and callback limits.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRateAdjustmentReason : uint8
{
	None UMETA(DisplayName = "No Adjustment", ToolTip = "The requested and resolved sample rates match."),
	ProjectPolicy UMETA(DisplayName = "Project Policy Limit", ToolTip = "OpenMobile Sensors Project Settings reduced the requested rate."),
	HardwareLimit UMETA(DisplayName = "Hardware Limit", ToolTip = "The device sensor cannot sustain the requested rate."),
	MissingPlatformDeclaration UMETA(DisplayName = "Platform Declaration Missing", ToolTip = "A required high-rate platform declaration is missing from the packaged application."),
	OperatingSystemLimit UMETA(DisplayName = "Operating System Limit", ToolTip = "The operating system capped the requested rate for this app or device."),
	BackendLimit UMETA(DisplayName = "Provider Limit", ToolTip = "The active sensor provider capped the requested rate.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRateResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double RequestedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double ClampedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double AppliedNativeFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorRateAdjustmentReason AdjustmentReason =
		EOpenMobileSensorRateAdjustmentReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FText AdjustmentExplanation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FText AdjustmentCorrection;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Whether another compatible listener raised the shared physical stream above this listener's resolved rate."))
	bool bSharedPhysicalStreamRateRaised = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Power warning produced when a faster compatible listener raises the shared physical stream rate."))
	FText SharedPhysicalStreamWarning;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Suggested action for reducing a shared physical stream rate."))
	FText SharedPhysicalStreamCorrection;
};

UENUM(BlueprintType)
enum class EOpenMobileSensorDeliveryMode : uint8
{
	LatestValue UMETA(DisplayName = "Latest Value", ToolTip = "Caches only the newest accepted sample for polling. Sample events are not fired."),
	EventBatches UMETA(DisplayName = "Event Batches", ToolTip = "Feeds listener and raw sample events with rate-capped batches."),
	Buffered UMETA(DisplayName = "Buffered Reads", ToolTip = "Keeps a bounded sample queue for explicit buffered reads. Sample events are not fired.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorCoordinateSpace : uint8
{
	DeviceFixed UMETA(DisplayName = "Device Fixed", ToolTip = "Uses stable Unreal device axes: X forward, Y right, and Z up, independent of screen rotation."),
	CurrentScreen UMETA(DisplayName = "Current Screen", ToolTip = "Rotates supported sensor values from device-fixed axes into the subsystem's current screen rotation.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorOverflowPolicy : uint8
{
	DropOldest UMETA(DisplayName = "Drop Oldest", ToolTip = "Removes the oldest queued sample to make room for each new sample."),
	RejectNewest UMETA(DisplayName = "Reject Newest", ToolTip = "Keeps the existing queue and rejects each new sample while the buffer is full.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorLifecyclePolicy : uint8
{
	SuspendInBackground UMETA(DisplayName = "Suspend in Background", ToolTip = "Pauses listener delivery while the application is inactive and resumes it on return."),
	StopInBackground UMETA(DisplayName = "Stop in Background", ToolTip = "Stops the listener when the application becomes inactive."),
	ContinueWhenSupported UMETA(DisplayName = "Continue When Supported", ToolTip = "Continues only when both project policy and the active platform sensor support background delivery.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorOptionIssueSeverity : uint8
{
	Warning UMETA(DisplayName = "Corrected Warning", ToolTip = "The field does not apply to this request and a safe value will be used."),
	Error UMETA(DisplayName = "Blocking Error", ToolTip = "The field applies to this request and must be corrected before the sensor can start.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorOptionsValidationOutcome : uint8
{
	Valid UMETA(DisplayName = "Valid", ToolTip = "The options can be used without correction."),
	Adjusted UMETA(DisplayName = "Valid with Corrections", ToolTip = "The options can be used after ignored invalid fields are replaced with safe values."),
	Invalid UMETA(DisplayName = "Invalid", ToolTip = "At least one active option must be corrected before the sensor can start.")
};

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Option Issue"))
struct OPENMOBILESENSORS_API FOpenMobileSensorOptionIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Exact stream-options field that needs attention."))
	FName Field;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Whether the field was corrected or blocks the request."))
	EOpenMobileSensorOptionIssueSeverity Severity =
		EOpenMobileSensorOptionIssueSeverity::Warning;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "User-facing explanation of the option problem."))
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Specific change that resolves the option problem."))
	FText Correction;
};

UENUM(BlueprintType)
enum class EOpenMobileActivityConfidence : uint8
{
	Unknown UMETA(DisplayName = "Unknown Confidence", ToolTip = "The provider did not report activity classification confidence."),
	Low UMETA(DisplayName = "Low Confidence", ToolTip = "The activity classification has low provider confidence."),
	Medium UMETA(DisplayName = "Medium Confidence", ToolTip = "The activity classification has moderate provider confidence."),
	High UMETA(DisplayName = "High Confidence", ToolTip = "The activity classification has high provider confidence.")
};

UENUM(BlueprintType)
enum class EOpenMobileAttitudeReferenceFrame : uint8
{
	GameRelative UMETA(DisplayName = "Game Relative", ToolTip = "Uses a recenterable game origin without requiring a north reference."),
	ArbitraryVertical UMETA(DisplayName = "Arbitrary Vertical", ToolTip = "Keeps gravity vertical while allowing yaw to drift around an arbitrary starting direction."),
	MagneticNorth UMETA(DisplayName = "Magnetic North", ToolTip = "References attitude yaw to magnetic north and may require magnetometer calibration."),
	TrueNorth UMETA(DisplayName = "True North", ToolTip = "References attitude yaw to true north and requires supported heading and location prerequisites.")
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileAttitudeRepresentation : uint8
{
	None = 0 UMETA(DisplayName = "Quaternion Only", ToolTip = "Keeps only the canonical quaternion that is present in every attitude sample."),
	Quaternion = 1 << 0 UMETA(DisplayName = "Quaternion Flag", ToolTip = "Requests the canonical normalized quaternion. It is already present in every attitude sample."),
	EulerAngles = 1 << 1 UMETA(DisplayName = "Euler Angles", ToolTip = "Also derives Unreal yaw, pitch, and roll in degrees for each subscriber."),
	RotationMatrix = 1 << 2 UMETA(DisplayName = "Rotation Matrix", ToolTip = "Also derives an orthonormal Unreal rotation matrix for each subscriber.")
};
ENUM_CLASS_FLAGS(EOpenMobileAttitudeRepresentation);

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorFilterOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	bool bEnableLowPass = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0001", ClampMax = "60.0", Units = "s"))
	double LowPassTimeConstantSeconds = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	bool bEnableHighPass = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0001", ClampMax = "60.0", Units = "s"))
	double HighPassTimeConstantSeconds = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	bool bEnableExponentialSmoothing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0001", ClampMax = "60.0", Units = "s"))
	double SmoothingTimeConstantSeconds = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0", ToolTip = "Uses vector magnitude in standardized units, or shortest angular distance to north in degrees for heading samples."))
	double DeadZone = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileShakeDetectionOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.1", ClampMax = "1000.0", Units = "m/s^2"))
	double StrengthThresholdMetresPerSecondSquared = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MinimumImpulses = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.01", ClampMax = "10.0", Units = "s"))
	double DurationWindowSeconds = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0", ClampMax = "5.0", Units = "s"))
	double QuietResetSeconds = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0", ClampMax = "60.0", Units = "s"))
	double CooldownSeconds = 1.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorStreamOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "1.0", ClampMax = "1000.0", Units = "Hz"))
	double CustomFrequencyHz = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0", ClampMax = "10.0", Units = "s"))
	double MaximumDeliveryLatencySeconds = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "1.0", ClampMax = "120.0", Units = "Hz"))
	double MaximumCallbackFrequencyHz = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileSensorAccuracy MinimumCallbackAccuracy =
		EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileActivityConfidence MinimumActivityConfidence =
		EOpenMobileActivityConfidence::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0", ClampMax = "3600.0", Units = "s"))
	double MinimumActivityStableDurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileSensorDeliveryMode DeliveryMode =
		EOpenMobileSensorDeliveryMode::LatestValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileSensorCoordinateSpace CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::DeviceFixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "1", ClampMax = "4096"))
	int32 BufferCapacitySamples = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileSensorOverflowPolicy OverflowPolicy =
		EOpenMobileSensorOverflowPolicy::DropOldest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ClampMin = "0.0"))
	double MinimumScalarEventChange = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	FOpenMobileSensorFilterOptions Filters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	FOpenMobileShakeDetectionOptions ShakeDetection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileSensorLifecyclePolicy LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::SuspendInBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileAttitudeRepresentation"))
	int32 AttitudeRepresentations =
		static_cast<int32>(EOpenMobileAttitudeRepresentation::Quaternion);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	bool bAllowDerivedFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	bool bAllowHighSamplingRate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	bool bLowLatency = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorSubscriptionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors")
	FOpenMobileSensorStreamOptions Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Treats a Step Counter subscription as an independently resettable count since subscription start."))
	bool bResettableStepCountSession = false;
};

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorRateLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Sensor Hertz to Interval Seconds", ToolTip = "Converts a finite positive sensor frequency to its sample interval."))
	static bool HertzToIntervalSeconds(
		double FrequencyHz,
		double& OutIntervalSeconds
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Sensor Interval Seconds to Hertz", ToolTip = "Converts a finite positive sample interval to sensor frequency."))
	static bool IntervalSecondsToHertz(
		double IntervalSeconds,
		double& OutFrequencyHz
	);
};

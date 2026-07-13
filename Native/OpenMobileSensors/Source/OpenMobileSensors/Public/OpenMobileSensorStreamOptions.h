#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorAccuracy.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorStreamOptions.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorRatePreset : uint8
{
	UI,
	Game,
	Fast,
	Custom
};

UENUM(BlueprintType)
enum class EOpenMobileSensorRateAdjustmentReason : uint8
{
	None,
	ProjectPolicy,
	HardwareLimit,
	MissingPlatformDeclaration,
	OperatingSystemLimit,
	BackendLimit
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRateResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double RequestedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double ClampedFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double AppliedNativeFrequencyHz = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorRateAdjustmentReason AdjustmentReason =
		EOpenMobileSensorRateAdjustmentReason::None;
};

UENUM(BlueprintType)
enum class EOpenMobileSensorDeliveryMode : uint8
{
	LatestValue,
	EventBatches,
	Buffered
};

UENUM(BlueprintType)
enum class EOpenMobileSensorCoordinateSpace : uint8
{
	DeviceFixed,
	CurrentScreen
};

UENUM(BlueprintType)
enum class EOpenMobileSensorOverflowPolicy : uint8
{
	DropOldest,
	RejectNewest
};

UENUM(BlueprintType)
enum class EOpenMobileSensorLifecyclePolicy : uint8
{
	SuspendInBackground,
	StopInBackground,
	ContinueWhenSupported
};

UENUM(BlueprintType)
enum class EOpenMobileActivityConfidence : uint8
{
	Unknown,
	Low,
	Medium,
	High
};

UENUM(BlueprintType)
enum class EOpenMobileAttitudeReferenceFrame : uint8
{
	GameRelative,
	ArbitraryVertical,
	MagneticNorth,
	TrueNorth
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileAttitudeRepresentation : uint8
{
	None = 0,
	Quaternion = 1 << 0,
	EulerAngles = 1 << 1,
	RotationMatrix = 1 << 2
};
ENUM_CLASS_FLAGS(EOpenMobileAttitudeRepresentation);

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorFilterOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bEnableLowPass = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ClampMin = "0.0001", ClampMax = "60.0", Units = "s"))
	double LowPassTimeConstantSeconds = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bEnableHighPass = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ClampMin = "0.0001", ClampMax = "60.0", Units = "s"))
	double HighPassTimeConstantSeconds = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bEnableExponentialSmoothing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double SmoothingTimeConstantSeconds = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double DeadZone = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorStreamOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double CustomFrequencyHz = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double MaximumDeliveryLatencySeconds = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	double MaximumCallbackFrequencyHz = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorAccuracy MinimumCallbackAccuracy =
		EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileActivityConfidence MinimumActivityConfidence =
		EOpenMobileActivityConfidence::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ClampMin = "0.0"))
	double MinimumActivityStableDurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorDeliveryMode DeliveryMode =
		EOpenMobileSensorDeliveryMode::LatestValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorCoordinateSpace CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::DeviceFixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	int32 BufferCapacitySamples = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorOverflowPolicy OverflowPolicy =
		EOpenMobileSensorOverflowPolicy::DropOldest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ClampMin = "0.0"))
	double MinimumScalarEventChange = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	FOpenMobileSensorFilterOptions Filters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorLifecyclePolicy LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::SuspendInBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileAttitudeRepresentation"))
	int32 AttitudeRepresentations =
		static_cast<int32>(EOpenMobileAttitudeRepresentation::Quaternion);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bAllowDerivedFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bAllowHighSamplingRate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bLowLatency = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorSubscriptionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	FOpenMobileSensorStreamOptions Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Treats a Step Counter subscription as an independently resettable count since subscription start."))
	bool bResettableStepCountSession = false;
};

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorRateLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Sensor Hertz to Interval Seconds", ToolTip = "Converts a finite positive sensor frequency to its sample interval."))
	static bool HertzToIntervalSeconds(
		double FrequencyHz,
		double& OutIntervalSeconds
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Sensor Interval Seconds to Hertz", ToolTip = "Converts a finite positive sample interval to sensor frequency."))
	static bool IntervalSecondsToHertz(
		double IntervalSeconds,
		double& OutFrequencyHz
	);
};

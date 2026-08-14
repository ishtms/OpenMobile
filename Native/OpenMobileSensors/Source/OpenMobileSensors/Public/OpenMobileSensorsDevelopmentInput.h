#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorsDevelopmentInput.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorsMockPreset : uint8
{
	Custom UMETA(DisplayName = "Custom Values", ToolTip = "Uses the mock values configured in OpenMobile Sensors Project Settings."),
	Stationary UMETA(DisplayName = "Stationary Device", ToolTip = "Uses deterministic values for a still device under normal gravity."),
	Walking UMETA(DisplayName = "Walking", ToolTip = "Uses deterministic motion, steps, and activity values representing walking."),
	Running UMETA(DisplayName = "Running", ToolTip = "Uses deterministic motion, steps, and activity values representing running."),
	Driving UMETA(DisplayName = "Driving", ToolTip = "Uses deterministic motion and automotive activity values."),
	PoorQuality UMETA(DisplayName = "Poor Sensor Quality", ToolTip = "Uses unreliable or degraded values for testing quality and calibration handling."),
	PermissionDenied UMETA(DisplayName = "Permission Denied", ToolTip = "Simulates denied sensor permission outcomes for development workflow tests.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorMockActionOutcome : uint8
{
	Applied UMETA(DisplayName = "Applied", ToolTip = "The development mock action was applied."),
	MocksInactive UMETA(DisplayName = "Mocks Inactive", ToolTip = "Mock input is disabled, unavailable, or excluded from this build."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "Mock input is active, but the requested development action failed.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorsMockInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Motion", meta = (ToolTip = "Acceleration in metres per second squared for this sensors mock input.", Units = "m/s^2"))
	FVector AccelerationMetresPerSecondSquared = FVector(0.0, 0.0, 9.80665);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Motion", meta = (ToolTip = "Angular velocity in radians per second for this sensors mock input.", Units = "rad/s"))
	FVector AngularVelocityRadiansPerSecond = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Motion", meta = (ToolTip = "Mock Unreal yaw, pitch, and roll attitude in degrees."))
	FRotator RotationDegrees = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Heading", meta = (ToolTip = "Mock heading in degrees clockwise from north.", ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	double HeadingDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Steps", meta = (ToolTip = "Mock cumulative step count. Decreases simulate a native counter reset.", ClampMin = "0"))
	int64 StepCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Activity", meta = (ToolTip = "Activity for this sensors mock input."))
	EOpenMobileMotionActivity Activity = EOpenMobileMotionActivity::Stationary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Activity", meta = (ToolTip = "Activity Confidence for this sensors mock input."))
	EOpenMobileActivityConfidence ActivityConfidence =
		EOpenMobileActivityConfidence::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Environment", meta = (ToolTip = "Pressure in hectopascals for this sensors mock input.", ClampMin = "0.0", DisplayName = "Pressure (hPa)"))
	double PressureHectopascals = 1013.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Environment", meta = (ToolTip = "Ambient light in lux for this sensors mock input.", ClampMin = "0.0", DisplayName = "Ambient Light (lux)"))
	double AmbientLightLux = 500.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Proximity", meta = (ToolTip = "Mock Near or Far proximity classification."))
	bool bProximityNear = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Proximity", meta = (ToolTip = "Proximity distance in metres for this sensors mock input.", ClampMin = "0.0", Units = "m"))
	double ProximityDistanceMeters = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Permissions", meta = (ToolTip = "Motion Activity Permission for this sensors mock input."))
	EOpenMobilePermissionStatus MotionActivityPermission =
		EOpenMobilePermissionStatus::Granted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Permissions", meta = (ToolTip = "Activity Recognition Permission for this sensors mock input."))
	EOpenMobilePermissionStatus ActivityRecognitionPermission =
		EOpenMobilePermissionStatus::Granted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Permissions", meta = (ToolTip = "True Heading Location Permission for this sensors mock input."))
	EOpenMobilePermissionStatus TrueHeadingLocationPermission =
		EOpenMobilePermissionStatus::Granted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Quality", meta = (ToolTip = "Whether the complete mock snapshot should pass sample validity checks."))
	bool bValuesValid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Quality", meta = (ToolTip = "Provider-reported accuracy classification for this value."))
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Quality", meta = (ToolTip = "Quality classification for the fused or derived value."))
	EOpenMobileSensorFusionQuality FusionQuality =
		EOpenMobileSensorFusionQuality::Nominal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Quality", meta = (ToolTip = "Simulates a provider calibration requirement and its listener event."))
	bool bCalibrationRequired = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Quality", meta = (ToolTip = "Mock sensor-specific uncertainty in the active sample family's standardized units.", ClampMin = "0.0"))
	double EstimatedError = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorsMockTimelineFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Timeline", meta = (ToolTip = "Timeline-relative time in seconds when this mock snapshot becomes active.", ClampMin = "0.0", Units = "s"))
	double TimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Timeline", meta = (ToolTip = "Complete mock sensor snapshot applied at Time Seconds."))
	FOpenMobileSensorsMockInput Input;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorsMockTimeline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Timeline", meta = (ToolTip = "Mock snapshots ordered by timeline-relative Time Seconds."))
	TArray<FOpenMobileSensorsMockTimelineFrame> Frames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Timeline", meta = (ToolTip = "Multiplier applied to automatic timeline advancement.", ClampMin = "0.01", ClampMax = "100.0"))
	double PlaybackSpeed = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Timeline", meta = (ToolTip = "Whether automatic or manual playback wraps to the first frame after the final frame."))
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Development|Timeline", meta = (ToolTip = "When true, timeline time advances only through the explicit development clock node."))
	bool bUseManualClock = false;
};

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorsDevelopmentLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Use this to apply one complete mock sensor snapshot in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Apply Sensor Mock Input", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock simulate test", ToolTip = "Applies one complete mock sensor snapshot in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void ApplyMockInputWithOutcome(
		const FOpenMobileSensorsMockInput& Input,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	/** Use this to apply a named sensor mock preset in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Apply Sensor Mock Preset", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock preset simulate test walking running", ToolTip = "Applies a named sensor mock preset in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void ApplyMockPresetWithOutcome(
		EOpenMobileSensorsMockPreset Preset,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	/** Use this to start a mock sensor timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Play Sensor Mock Timeline", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline sequence simulate test", ToolTip = "Starts a mock sensor timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void PlayMockTimelineWithOutcome(
		const FOpenMobileSensorsMockTimeline& Timeline,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	/** Use this to stop the active mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Stop Sensor Mock Timeline", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline stop simulate test", ToolTip = "Stops the active mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void StopMockTimelineWithOutcome(
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	/** Use this to advance a manual-clock mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Advance Sensor Mock Timeline", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline manual clock advance test", ToolTip = "Advances a manual-clock mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void AdvanceMockTimelineWithOutcome(
		double DeltaSeconds,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	/** Use this to inject one normalized sensor failure in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Inject Sensor Mock Error", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock error failure inject test", ToolTip = "Injects one normalized sensor failure in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void InjectMockErrorWithOutcome(
		EOpenMobileSensorType Sensor,
		EOpenMobileSensorFailureReason FailureReason,
		FString NativeCode,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	/** You'll only need this raw-result path for applying mock input in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Apply Sensor Mock Input (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock raw result", ToolTip = "Advanced raw-result path for applying mock input in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult ApplyMockInput(
		const FOpenMobileSensorsMockInput& Input
	);

	/** You'll only need this raw-result path for applying a mock preset in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Apply Sensor Mock Preset (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock preset raw result", ToolTip = "Advanced raw-result path for applying a mock preset in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult ApplyMockPreset(
		EOpenMobileSensorsMockPreset Preset
	);

	/** You'll only need this raw-result path for playing a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Play Sensor Mock Timeline (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline raw result", ToolTip = "Advanced raw-result path for playing a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult PlayMockTimeline(
		const FOpenMobileSensorsMockTimeline& Timeline
	);

	/** You'll only need this raw-result path for stopping a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Stop Sensor Mock Timeline (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline stop raw result", ToolTip = "Advanced raw-result path for stopping a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult StopMockTimeline();

	/** You'll only need this raw-result path for advancing a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Advance Sensor Mock Timeline (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline manual clock raw result", ToolTip = "Advanced raw-result path for advancing a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult AdvanceMockTimeline(
		double DeltaSeconds
	);

	/** You'll only need this raw-result path for injecting a sensor failure in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Inject Sensor Mock Error (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock error failure raw result", ToolTip = "Advanced raw-result path for injecting a sensor failure in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult InjectMockError(
		EOpenMobileSensorType Sensor,
		EOpenMobileSensorFailureReason FailureReason,
		FString NativeCode
	);

	/** Use this to check whether the non-Shipping mock provider is currently active. Always returns false in Shipping builds. Configure Project Settings > OpenMobile > OpenMobile Sensors. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Is Sensor Mock Input Active", DevelopmentOnly, Keywords = "OpenMobile sensors development mock enabled status", ToolTip = "Checks whether the non-Shipping mock provider is currently active. Always returns false in Shipping builds. Configure Project Settings > OpenMobile > OpenMobile Sensors."))
	static bool IsMockInputActive();
};

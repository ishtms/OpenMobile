#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorsDevelopmentInput.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorsMockPreset : uint8
{
	Custom,
	Stationary,
	Walking,
	Running,
	Driving,
	PoorQuality,
	PermissionDenied
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (Units = "m/s^2"))
	FVector AccelerationMetresPerSecondSquared = FVector(0.0, 0.0, 9.80665);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (Units = "rad/s"))
	FVector AngularVelocityRadiansPerSecond = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	FRotator RotationDegrees = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heading", meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	double HeadingDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steps", meta = (ClampMin = "0"))
	int64 StepCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	EOpenMobileMotionActivity Activity = EOpenMobileMotionActivity::Stationary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	EOpenMobileActivityConfidence ActivityConfidence =
		EOpenMobileActivityConfidence::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0.0", DisplayName = "Pressure (hPa)"))
	double PressureHectopascals = 1013.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0.0", DisplayName = "Ambient Light (lux)"))
	double AmbientLightLux = 500.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Proximity")
	bool bProximityNear = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Proximity", meta = (ClampMin = "0.0", Units = "m"))
	double ProximityDistanceMeters = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Permissions")
	EOpenMobilePermissionStatus MotionActivityPermission =
		EOpenMobilePermissionStatus::Granted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Permissions")
	EOpenMobilePermissionStatus ActivityRecognitionPermission =
		EOpenMobilePermissionStatus::Granted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Permissions")
	EOpenMobilePermissionStatus TrueHeadingLocationPermission =
		EOpenMobilePermissionStatus::Granted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	bool bValuesValid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	EOpenMobileSensorFusionQuality FusionQuality =
		EOpenMobileSensorFusionQuality::Nominal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	bool bCalibrationRequired = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality", meta = (ClampMin = "0.0"))
	double EstimatedError = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorsMockTimelineFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ClampMin = "0.0", Units = "s"))
	double TimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	FOpenMobileSensorsMockInput Input;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorsMockTimeline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileSensorsMockTimelineFrame> Frames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ClampMin = "0.01", ClampMax = "100.0"))
	double PlaybackSpeed = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	bool bUseManualClock = false;
};

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorsDevelopmentLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Apply Sensor Mock Input", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock simulate test", ToolTip = "Applies one complete mock sensor snapshot in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void ApplyMockInputWithOutcome(
		const FOpenMobileSensorsMockInput& Input,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Apply Sensor Mock Preset", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock preset simulate test walking running", ToolTip = "Applies a named sensor mock preset in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void ApplyMockPresetWithOutcome(
		EOpenMobileSensorsMockPreset Preset,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Play Sensor Mock Timeline", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline sequence simulate test", ToolTip = "Starts a mock sensor timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void PlayMockTimelineWithOutcome(
		const FOpenMobileSensorsMockTimeline& Timeline,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Stop Sensor Mock Timeline", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline stop simulate test", ToolTip = "Stops the active mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void StopMockTimelineWithOutcome(
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Advance Sensor Mock Timeline", ExpandEnumAsExecs = "Outcome", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline manual clock advance test", ToolTip = "Advances a manual-clock mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static void AdvanceMockTimelineWithOutcome(
		double DeltaSeconds,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

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

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Apply Sensor Mock Input (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock raw result", ToolTip = "Advanced raw-result path for applying mock input in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult ApplyMockInput(
		const FOpenMobileSensorsMockInput& Input
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Apply Sensor Mock Preset (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock preset raw result", ToolTip = "Advanced raw-result path for applying a mock preset in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult ApplyMockPreset(
		EOpenMobileSensorsMockPreset Preset
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Play Sensor Mock Timeline (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline raw result", ToolTip = "Advanced raw-result path for playing a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult PlayMockTimeline(
		const FOpenMobileSensorsMockTimeline& Timeline
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Stop Sensor Mock Timeline (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline stop raw result", ToolTip = "Advanced raw-result path for stopping a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult StopMockTimeline();

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Advance Sensor Mock Timeline (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock timeline manual clock raw result", ToolTip = "Advanced raw-result path for advancing a mock timeline in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult AdvanceMockTimeline(
		double DeltaSeconds
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only|Advanced", meta = (DisplayName = "Inject Sensor Mock Error (Advanced)", DevelopmentOnly, Keywords = "OpenMobile sensors development mock error failure raw result", ToolTip = "Advanced raw-result path for injecting a sensor failure in non-Shipping builds. Enable Mock in Project Settings > OpenMobile > OpenMobile Sensors."))
	static FOpenMobileSensorOperationResult InjectMockError(
		EOpenMobileSensorType Sensor,
		EOpenMobileSensorFailureReason FailureReason,
		FString NativeCode
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Development Only", meta = (DisplayName = "Is Sensor Mock Input Active", DevelopmentOnly, Keywords = "OpenMobile sensors development mock enabled status", ToolTip = "Checks whether the non-Shipping mock provider is currently active. Always returns false in Shipping builds. Configure Project Settings > OpenMobile > OpenMobile Sensors."))
	static bool IsMockInputActive();
};

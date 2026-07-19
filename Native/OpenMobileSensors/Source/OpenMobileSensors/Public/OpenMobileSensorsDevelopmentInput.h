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
	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors|Development", meta = (DisplayName = "Apply Sensor Mock Input"))
	static FOpenMobileSensorOperationResult ApplyMockInput(
		const FOpenMobileSensorsMockInput& Input
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors|Development", meta = (DisplayName = "Apply Sensor Mock Preset"))
	static FOpenMobileSensorOperationResult ApplyMockPreset(
		EOpenMobileSensorsMockPreset Preset
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors|Development", meta = (DisplayName = "Play Sensor Mock Timeline"))
	static FOpenMobileSensorOperationResult PlayMockTimeline(
		const FOpenMobileSensorsMockTimeline& Timeline
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors|Development", meta = (DisplayName = "Stop Sensor Mock Timeline"))
	static FOpenMobileSensorOperationResult StopMockTimeline();

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors|Development", meta = (DisplayName = "Advance Sensor Mock Timeline"))
	static FOpenMobileSensorOperationResult AdvanceMockTimeline(
		double DeltaSeconds
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors|Development", meta = (DisplayName = "Inject Sensor Mock Error"))
	static FOpenMobileSensorOperationResult InjectMockError(
		EOpenMobileSensorType Sensor,
		EOpenMobileSensorFailureReason FailureReason,
		FString NativeCode
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors|Development", meta = (DisplayName = "Is Sensor Mock Input Active"))
	static bool IsMockInputActive();
};

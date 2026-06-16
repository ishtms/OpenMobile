#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorAccuracy.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorQuality.h"
#include "OpenMobileSensorScreenRotation.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorSamples.generated.h"

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileSensorTimestampIssue : uint8
{
	None = 0,
	Invalid = 1 << 0,
	Duplicate = 1 << 1,
	Backward = 1 << 2
};
ENUM_CLASS_FLAGS(EOpenMobileSensorTimestampIssue);

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorSampleHeader
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double GameThreadReceiptSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasGameThreadReceiptTime = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorTimestampIssue"))
	int32 TimestampIssueFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bStatefulProcessingReset = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bUnitsNormalized = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bCoordinatesNormalized = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorCoordinateSpace CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::DeviceFixed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorScreenRotation ScreenRotation =
		EOpenMobileSensorScreenRotation::Rotation0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double ScreenRotationTimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 ScreenRotationSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bNaturalOrientationLandscape = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorSourceFlags"))
	int32 SourceFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bSourceChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorFusionContext Fusion;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasEstimatedError = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double EstimatedError = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileVectorSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FVector Value = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasBias = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FVector Bias = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRotationMatrix
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FVector XAxis = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FVector YAxis = FVector::RightVector;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FVector ZAxis = FVector::UpVector;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FQuat Quaternion = FQuat::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasEulerDegrees = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FRotator EulerDegrees = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasRotationMatrix = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorRotationMatrix RotationMatrix;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileAttitudeReferenceFrame ReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorFusionQuality FusionQuality =
		EOpenMobileSensorFusionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<EOpenMobileSensorType> ContributingSensors;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileScalarSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double Value = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileHeadingSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double HeadingDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasAccuracyDegrees = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double AccuracyDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bCalibrationRequired = false;
};

UENUM(BlueprintType)
enum class EOpenMobileStepCountOrigin : uint8
{
	Unknown,
	DeviceBoot,
	QueryInterval,
	Session
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobilePedometerMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasDistanceMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double DistanceMeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasFloorsAscended = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 FloorsAscended = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasFloorsDescended = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 FloorsDescended = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasPaceSecondsPerMeter = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double PaceSecondsPerMeter = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasCadenceStepsPerSecond = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double CadenceStepsPerSecond = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileStepsSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	int64 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileStepCountOrigin Origin = EOpenMobileStepCountOrigin::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobilePedometerMetrics Metrics;
};

UENUM(BlueprintType)
enum class EOpenMobileMotionActivity : uint8
{
	Unknown,
	Stationary,
	Walking,
	Running,
	Cycling,
	Automotive
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
enum class EOpenMobileActivityTransition : uint8
{
	None,
	Started,
	Stopped
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileActivitySensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileMotionActivity Activity = EOpenMobileMotionActivity::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileActivityConfidence Confidence =
		EOpenMobileActivityConfidence::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileActivityTransition Transition =
		EOpenMobileActivityTransition::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<EOpenMobileMotionActivity> ConcurrentActivities;
};

UENUM(BlueprintType)
enum class EOpenMobilePhysicalOrientation : uint8
{
	Unknown,
	Portrait,
	PortraitUpsideDown,
	LandscapeLeft,
	LandscapeRight,
	FaceUp,
	FaceDown
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileOrientationSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobilePhysicalOrientation Orientation =
		EOpenMobilePhysicalOrientation::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double Confidence = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileProximitySensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bNear = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasDistanceMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double DistanceMeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasMaximumRangeMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double MaximumRangeMeters = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileVectorSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileVectorSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileAttitudeSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileScalarSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileScalarSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileHeadingSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileHeadingSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileStepsSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileStepsSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileActivitySensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileActivitySensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileOrientationSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileOrientationSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileProximitySensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	TArray<FOpenMobileProximitySensorSample> Samples;
};

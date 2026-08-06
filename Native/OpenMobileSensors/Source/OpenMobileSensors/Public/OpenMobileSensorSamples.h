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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double GameThreadReceiptSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasGameThreadReceiptTime = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorTimestampIssue"))
	int32 TimestampIssueFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bStatefulProcessingReset = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bUnitsNormalized = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bCoordinatesNormalized = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorCoordinateSpace CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::DeviceFixed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorScreenRotation ScreenRotation =
		EOpenMobileSensorScreenRotation::Rotation0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double ScreenRotationTimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 ScreenRotationSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bNaturalOrientationLandscape = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorSourceFlags"))
	int32 SourceFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bSourceChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorFusionContext Fusion;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasEstimatedError = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double EstimatedError = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileShakeEventData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double StrengthMetresPerSecondSquared = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int32 ImpulseCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorIdentifier SourceSensor;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileVectorSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FVector Value = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasBias = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FVector Bias = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHighPassFiltered = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHighPassFilterWarmingUp = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bExponentiallySmoothed = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bDeadZoneSuppressed = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasShakeEvent = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileShakeEventData ShakeEvent;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRotationMatrix
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FVector XAxis = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FVector YAxis = FVector::RightVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FVector ZAxis = FVector::UpVector;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FQuat Quaternion = FQuat::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasEulerDegrees = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FRotator EulerDegrees = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasRotationMatrix = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorRotationMatrix RotationMatrix;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileAttitudeReferenceFrame ReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFusionQuality FusionQuality =
		EOpenMobileSensorFusionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<EOpenMobileSensorType> ContributingSensors;
};

UENUM(BlueprintType)
enum class EOpenMobileRelativeAltitudeSource : uint8
{
	None,
	NativePlatform,
	PressureBaseline
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileRelativeAltitudeQualityLimitation : uint8
{
	None = 0,
	WeatherSensitive = 1 << 0,
	StandardAtmosphereAssumption = 1 << 1,
	NativeModelUnspecified = 1 << 2
};
ENUM_CLASS_FLAGS(EOpenMobileRelativeAltitudeQualityLimitation);

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileRelativeAltitudeMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileRelativeAltitudeSource Source =
		EOpenMobileRelativeAltitudeSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double BaselineTimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasBaselinePressure = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double BaselinePressureHectopascals = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bUsesStandardAtmosphereModel = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileRelativeAltitudeQualityLimitation"))
	int32 QualityLimitationFlags = 0;
};

UENUM(BlueprintType)
enum class EOpenMobileAbsoluteAltitudeSource : uint8
{
	None,
	NativePlatform
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAbsoluteAltitudeMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileAbsoluteAltitudeSource Source =
		EOpenMobileAbsoluteAltitudeSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasVerticalAccuracy = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw vertical-accuracy backing value in metres. Read only when Has Vertical Accuracy is true."))
	double VerticalAccuracyMeters = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileScalarSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double Value = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileRelativeAltitudeMetadata RelativeAltitude;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileAbsoluteAltitudeMetadata AbsoluteAltitude;
};

UENUM(BlueprintType)
enum class EOpenMobileHeadingReference : uint8
{
	Unknown,
	MagneticNorth,
	TrueNorth
};

UENUM(BlueprintType)
enum class EOpenMobileHeadingDeclinationSource : uint8
{
	None,
	WorldMagneticModel2025,
	NativePlatform
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileHeadingSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double HeadingDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileHeadingReference Reference =
		EOpenMobileHeadingReference::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileHeadingDeclinationSource DeclinationSource =
		EOpenMobileHeadingDeclinationSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasDeclinationDegrees = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double DeclinationDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasLocationAgeSeconds = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double LocationAgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bTiltCompensated = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasAccuracyDegrees = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw heading-accuracy backing value in degrees. Read only when Has Accuracy Degrees is true."))
	double AccuracyDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bExponentiallySmoothed = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bDeadZoneSuppressed = false;
};

UENUM(BlueprintType)
enum class EOpenMobileStepCountOrigin : uint8
{
	Unknown,
	DeviceBoot,
	QueryInterval,
	Session
};

UENUM(BlueprintType)
enum class EOpenMobileStepCountDiscontinuity : uint8
{
	None,
	StreamStarted,
	NativeCounterReset,
	OriginChanged,
	SessionReset
};

UENUM(BlueprintType)
enum class EOpenMobileStepDetectionSource : uint8
{
	Unknown,
	AndroidStepDetector,
	IOSPedometerDelta
};

UENUM(BlueprintType)
enum class EOpenMobileStepDetectionQuality : uint8
{
	Unknown,
	DirectHardwareEvent,
	InferredFromPedometerDelta
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobilePedometerMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasDistanceMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw distance backing value in metres. Read only when Has Distance Meters is true."))
	double DistanceMeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasFloorsAscended = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw ascending-floor count. Read only when Has Floors Ascended is true."))
	int64 FloorsAscended = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasFloorsDescended = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw descending-floor count. Read only when Has Floors Descended is true."))
	int64 FloorsDescended = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasPaceSecondsPerMeter = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double PaceSecondsPerMeter = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasCadenceStepsPerSecond = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double CadenceStepsPerSecond = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileStepsSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	int64 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileStepCountOrigin Origin = EOpenMobileStepCountOrigin::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FGuid OriginIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileStepCountDiscontinuity Discontinuity =
		EOpenMobileStepCountDiscontinuity::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bCountSaturated = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Number of newly detected steps represented by this StepDetector event."))
	int64 DetectedStepDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasNativeTotal = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Platform cumulative total used to derive the event when available."))
	int64 NativeTotal = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileStepDetectionSource DetectionSource =
		EOpenMobileStepDetectionSource::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileStepDetectionQuality DetectionQuality =
		EOpenMobileStepDetectionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasQueryInterval = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Start of the native query interval in Unix time seconds."))
	double QueryStartUnixTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "End of the native query interval in Unix time seconds."))
	double QueryEndUnixTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
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
enum class EOpenMobileActivityTransition : uint8
{
	None,
	Started,
	Stopped
};

UENUM(BlueprintType)
enum class EOpenMobileActivityTransitionOrigin : uint8
{
	Unknown,
	Native,
	Derived
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileActivitySensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileMotionActivity Activity = EOpenMobileMotionActivity::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileActivityConfidence Confidence =
		EOpenMobileActivityConfidence::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileActivityTransition Transition =
		EOpenMobileActivityTransition::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileActivityTransitionOrigin TransitionOrigin =
		EOpenMobileActivityTransitionOrigin::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FName ActivityProvider;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobilePhysicalOrientation Orientation =
		EOpenMobilePhysicalOrientation::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	double Confidence = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileProximitySensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bNear = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasDistanceMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw proximity-distance backing value in metres. Read only when Has Distance Meters is true."))
	double DistanceMeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasMaximumRangeMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw proximity-range backing value in metres. Read only when Has Maximum Range Meters is true."))
	double MaximumRangeMeters = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileVectorSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileVectorSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileAttitudeSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileScalarSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileScalarSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileHeadingSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileHeadingSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileStepsSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileStepsSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileActivitySensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileActivitySensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileOrientationSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileOrientationSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileProximitySensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	TArray<FOpenMobileProximitySensorSample> Samples;
};

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
	None = 0 UMETA(DisplayName = "No Timestamp Issue", ToolTip = "The sample timestamp is finite and later than the previous accepted timestamp."),
	Invalid = 1 << 0 UMETA(DisplayName = "Invalid Timestamp", ToolTip = "The provider supplied a nonfinite or otherwise invalid monotonic timestamp."),
	Duplicate = 1 << 1 UMETA(DisplayName = "Duplicate Timestamp", ToolTip = "The timestamp matches the previous accepted sample timestamp."),
	Backward = 1 << 2 UMETA(DisplayName = "Backward Timestamp", ToolTip = "The timestamp is earlier than the previous accepted sample timestamp.")
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
	None UMETA(DisplayName = "No Source", ToolTip = "No relative-altitude provider produced this sample."),
	NativePlatform UMETA(DisplayName = "Native Platform", ToolTip = "The platform's native relative-altitude API produced the value."),
	PressureBaseline UMETA(DisplayName = "Pressure Baseline", ToolTip = "OpenMobile Sensors derived displacement from barometric pressure relative to a captured baseline.")
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileRelativeAltitudeQualityLimitation : uint8
{
	None = 0 UMETA(DisplayName = "No Known Limitation", ToolTip = "No additional relative-altitude quality limitation was reported."),
	WeatherSensitive = 1 << 0 UMETA(DisplayName = "Weather Sensitive", ToolTip = "Atmospheric pressure changes unrelated to movement can shift the reported altitude."),
	StandardAtmosphereAssumption = 1 << 1 UMETA(DisplayName = "Standard Atmosphere Assumption", ToolTip = "Pressure conversion assumes a standard atmosphere and is not survey-grade elevation."),
	NativeModelUnspecified = 1 << 2 UMETA(DisplayName = "Native Model Unspecified", ToolTip = "The platform does not document the model used for its relative-altitude value.")
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
	None UMETA(DisplayName = "No Source", ToolTip = "No absolute-altitude provider produced this sample."),
	NativePlatform UMETA(DisplayName = "Native Platform", ToolTip = "The platform's native altitude service produced the value and accuracy estimate.")
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
	Unknown UMETA(DisplayName = "Unknown Reference", ToolTip = "The provider did not identify the heading's north reference."),
	MagneticNorth UMETA(DisplayName = "Magnetic North", ToolTip = "Heading is measured in degrees clockwise from magnetic north."),
	TrueNorth UMETA(DisplayName = "True North", ToolTip = "Heading is measured in degrees clockwise from geographic true north.")
};

UENUM(BlueprintType)
enum class EOpenMobileHeadingDeclinationSource : uint8
{
	None UMETA(DisplayName = "No Declination", ToolTip = "No magnetic declination correction was applied."),
	WorldMagneticModel2025 UMETA(DisplayName = "World Magnetic Model 2025", ToolTip = "The plugin calculated declination from the 2025 World Magnetic Model and the supplied location and time."),
	NativePlatform UMETA(DisplayName = "Native Platform", ToolTip = "The platform supplied true heading or its own declination correction.")
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
	Unknown UMETA(DisplayName = "Unknown Origin", ToolTip = "The provider did not identify the step count's zero point."),
	DeviceBoot UMETA(DisplayName = "Since Device Boot", ToolTip = "The count is cumulative from the platform's device-boot counter and may reset after reboot."),
	QueryInterval UMETA(DisplayName = "Query Interval", ToolTip = "The count covers the explicit start and end time of a native historical query."),
	Session UMETA(DisplayName = "Listener Session", ToolTip = "The count starts from the listener session baseline and can be reset explicitly.")
};

UENUM(BlueprintType)
enum class EOpenMobileStepCountDiscontinuity : uint8
{
	None UMETA(DisplayName = "No Discontinuity", ToolTip = "The step count continues from the prior sample without a known baseline change."),
	StreamStarted UMETA(DisplayName = "Stream Started", ToolTip = "This is the first count after starting or restarting the step stream."),
	NativeCounterReset UMETA(DisplayName = "Native Counter Reset", ToolTip = "The platform cumulative counter decreased or reset, commonly after a device reboot."),
	OriginChanged UMETA(DisplayName = "Count Origin Changed", ToolTip = "The provider changed the meaning of the count's zero point."),
	SessionReset UMETA(DisplayName = "Session Reset", ToolTip = "Blueprint explicitly reset the typed step listener's session baseline.")
};

UENUM(BlueprintType)
enum class EOpenMobileStepDetectionSource : uint8
{
	Unknown UMETA(DisplayName = "Unknown Source", ToolTip = "The provider did not identify how the step event was detected."),
	AndroidStepDetector UMETA(DisplayName = "Android Step Detector", ToolTip = "Android's native discrete step detector produced the event."),
	IOSPedometerDelta UMETA(DisplayName = "iOS Pedometer Delta", ToolTip = "The plugin inferred the event from an increase in Core Motion pedometer count.")
};

UENUM(BlueprintType)
enum class EOpenMobileStepDetectionQuality : uint8
{
	Unknown UMETA(DisplayName = "Unknown Quality", ToolTip = "The provider did not report whether detection was direct or inferred."),
	DirectHardwareEvent UMETA(DisplayName = "Direct Hardware Event", ToolTip = "A native discrete detector reported this step directly."),
	InferredFromPedometerDelta UMETA(DisplayName = "Inferred from Pedometer", ToolTip = "The plugin inferred this step from a cumulative pedometer count change.")
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
	Unknown UMETA(DisplayName = "Unknown Activity", ToolTip = "The provider could not classify the current motion activity."),
	Stationary UMETA(DisplayName = "Stationary", ToolTip = "The device is classified as not moving meaningfully."),
	Walking UMETA(DisplayName = "Walking", ToolTip = "The device is classified as moving with a walking pattern."),
	Running UMETA(DisplayName = "Running", ToolTip = "The device is classified as moving with a running pattern."),
	Cycling UMETA(DisplayName = "Cycling", ToolTip = "The device is classified as moving by bicycle."),
	Automotive UMETA(DisplayName = "Automotive", ToolTip = "The device is classified as moving in a motor vehicle.")
};

UENUM(BlueprintType)
enum class EOpenMobileActivityTransition : uint8
{
	None UMETA(DisplayName = "No Transition", ToolTip = "The sample does not represent an activity state transition."),
	Started UMETA(DisplayName = "Activity Started", ToolTip = "The classified activity became active."),
	Stopped UMETA(DisplayName = "Activity Stopped", ToolTip = "The classified activity ceased to be active.")
};

UENUM(BlueprintType)
enum class EOpenMobileActivityTransitionOrigin : uint8
{
	Unknown UMETA(DisplayName = "Unknown Origin", ToolTip = "The provider did not identify how the transition was produced."),
	Native UMETA(DisplayName = "Native Transition", ToolTip = "The platform reported the activity transition directly."),
	Derived UMETA(DisplayName = "Plugin Derived Transition", ToolTip = "OpenMobile Sensors derived the transition from classified activity samples.")
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
	Unknown UMETA(DisplayName = "Unknown Orientation", ToolTip = "The device posture is ambiguous or has not been classified yet."),
	Portrait UMETA(DisplayName = "Portrait", ToolTip = "The device is upright in its natural portrait orientation."),
	PortraitUpsideDown UMETA(DisplayName = "Portrait Upside Down", ToolTip = "The device is upright with its natural portrait orientation inverted."),
	LandscapeLeft UMETA(DisplayName = "Landscape Left", ToolTip = "The device is held in landscape with its left edge oriented downward."),
	LandscapeRight UMETA(DisplayName = "Landscape Right", ToolTip = "The device is held in landscape with its right edge oriented downward."),
	FaceUp UMETA(DisplayName = "Face Up", ToolTip = "The device screen is approximately horizontal and facing upward."),
	FaceDown UMETA(DisplayName = "Face Down", ToolTip = "The device screen is approximately horizontal and facing downward.")
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

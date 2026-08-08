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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor type and provider instance this value describes."))
	FOpenMobileSensorIdentifier Sensor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonic sensor-service timestamp in seconds for this value."))
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Timing", meta = (ToolTip = "Engine monotonic time in seconds when this sample reached the game thread. Read only when Has Game Thread Receipt Time is true."))
	double GameThreadReceiptSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Timing", meta = (ToolTip = "Whether Game Thread Receipt Seconds contains a valid engine monotonic timestamp."))
	bool bHasGameThreadReceiptTime = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonically increasing sequence used to order values from this source."))
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Timing", meta = (ToolTip = "Timestamp validation flags. Invalid samples do not replace the latest accepted sample.", Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorTimestampIssue"))
	int32 TimestampIssueFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Timing", meta = (ToolTip = "Whether filters and derived state restarted before this sample because of a gap, lifecycle change, source change, or invalid input."))
	bool bStatefulProcessingReset = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Normalization", meta = (ToolTip = "Whether numeric values use the standardized OpenMobile Sensors units documented for this sensor type."))
	bool bUnitsNormalized = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Normalization", meta = (ToolTip = "Whether vector and orientation values use Unreal device axes before optional screen compensation."))
	bool bCoordinatesNormalized = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Validity", meta = (ToolTip = "Whether this sample passed finite-value, timestamp, range, and sensor-specific validation."))
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Provider-reported accuracy classification for this value."))
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Quality", meta = (ToolTip = "Whether the provider currently recommends or requires calibration for reliable readings."))
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Coordinates", meta = (ToolTip = "Coordinate space applied to this sample after device-axis normalization."))
	EOpenMobileSensorCoordinateSpace CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::DeviceFixed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Coordinates", meta = (ToolTip = "Screen rotation applied to this sample. Rotation 0 is recorded for device-fixed output."))
	EOpenMobileSensorScreenRotation ScreenRotation =
		EOpenMobileSensorScreenRotation::Rotation0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Coordinates", meta = (ToolTip = "Engine monotonic time in seconds when the applied screen rotation became current."))
	double ScreenRotationTimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Coordinates", meta = (ToolTip = "Sequence of the screen-rotation snapshot used for this sample."))
	int64 ScreenRotationSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Coordinates", meta = (ToolTip = "Whether the device reports landscape as its natural display orientation."))
	bool bNaturalOrientationLandscape = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Typed provenance flags describing how the value was produced.", Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorSourceFlags"))
	int32 SourceFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Provenance", meta = (ToolTip = "Whether the sample source changed since the previous accepted sample, such as native to derived or live to replay."))
	bool bSourceChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Fusion quality, source inputs, and estimated processing lag for this sample."))
	FOpenMobileSensorFusionContext Fusion;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Quality", meta = (ToolTip = "Whether Estimated Error contains a finite sensor-specific uncertainty value."))
	bool bHasEstimatedError = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Quality", meta = (ToolTip = "Sensor-specific estimated uncertainty in the sample family's standardized units. Read only when Has Estimated Error is true."))
	double EstimatedError = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileShakeEventData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Strength in metres per second squared for this shake event data."))
	double StrengthMetresPerSecondSquared = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Duration in seconds for this shake event data."))
	double DurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Monotonic sensor-service timestamp in seconds for this value."))
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Impulse Count for this shake event data."))
	int32 ImpulseCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Source Sensor for this shake event data."))
	FOpenMobileSensorIdentifier SourceSensor;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileVectorSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor identity, timing, validity, quality, and provenance shared by this sample."))
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Value", meta = (ToolTip = "Normalized vector value. Units depend on Sensor Type: acceleration and gravity use m/s^2, gyroscope uses rad/s, and magnetometer uses microteslas."))
	FVector Value = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when bias is present and safe to read."))
	bool bHasBias = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Value", meta = (AdvancedDisplay, ToolTip = "Native bias estimate in the same units and axes as Value. Read only when Has Bias is true."))
	FVector Bias = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Processing", meta = (ToolTip = "Whether the configured high-pass filter produced this vector. This flag does not mean gravity was removed."))
	bool bHighPassFiltered = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Processing", meta = (ToolTip = "Whether the high-pass filter has accumulated less than one configured time constant of accepted samples."))
	bool bHighPassFilterWarmingUp = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Processing", meta = (ToolTip = "Whether exponential smoothing was enabled for this subscriber."))
	bool bExponentiallySmoothed = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Processing", meta = (ToolTip = "Whether the configured dead zone suppressed an output before this delivered sample."))
	bool bDeadZoneSuppressed = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when shake event is present and safe to read."))
	bool bHasShakeEvent = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Shake Event for this vector sensor sample."))
	FOpenMobileShakeEventData ShakeEvent;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRotationMatrix
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (ToolTip = "Rotated Unreal forward axis. The three axes form an orthonormal matrix."))
	FVector XAxis = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (ToolTip = "Rotated Unreal right axis. The three axes form an orthonormal matrix."))
	FVector YAxis = FVector::RightVector;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (ToolTip = "Rotated Unreal up axis. The three axes form an orthonormal matrix."))
	FVector ZAxis = FVector::UpVector;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor identity, timing, validity, quality, and provenance shared by this sample."))
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (ToolTip = "Canonical normalized Unreal orientation quaternion. Quaternion and its negation represent the same orientation."))
	FQuat Quaternion = FQuat::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (ToolTip = "Whether Euler Degrees was requested and calculated for this subscriber."))
	bool bHasEulerDegrees = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (AdvancedDisplay, ToolTip = "Unreal yaw, pitch, and roll in degrees. Read only when Has Euler Degrees is true."))
	FRotator EulerDegrees = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (ToolTip = "Whether Rotation Matrix was requested and calculated for this subscriber."))
	bool bHasRotationMatrix = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (AdvancedDisplay, ToolTip = "Orthonormal Unreal rotation axes. Read only when Has Rotation Matrix is true."))
	FOpenMobileSensorRotationMatrix RotationMatrix;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (ToolTip = "Attitude reference frame actually applied by the provider."))
	EOpenMobileAttitudeReferenceFrame ReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Quality classification for the fused or derived value."))
	EOpenMobileSensorFusionQuality FusionQuality =
		EOpenMobileSensorFusionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Attitude", meta = (AdvancedDisplay, ToolTip = "Sensor types known to contribute to this fused orientation. Empty when the native provider does not disclose them."))
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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Source for this relative altitude metadata."))
	EOpenMobileRelativeAltitudeSource Source =
		EOpenMobileRelativeAltitudeSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Altitude", meta = (ToolTip = "Sensor monotonic time in seconds when the relative-altitude baseline was captured."))
	double BaselineTimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Altitude", meta = (ToolTip = "Whether Baseline Pressure Hectopascals is available for a pressure-derived altitude sample."))
	bool bHasBaselinePressure = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Altitude", meta = (AdvancedDisplay, ToolTip = "Atmospheric pressure in hectopascals captured at the relative zero point. Read only when Has Baseline Pressure is true."))
	double BaselinePressureHectopascals = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Altitude", meta = (ToolTip = "Whether pressure conversion assumes a standard atmosphere rather than measured local weather conditions."))
	bool bUsesStandardAtmosphereModel = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Altitude", meta = (AdvancedDisplay, ToolTip = "Known limitations of the native or pressure-derived altitude model.", Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileRelativeAltitudeQualityLimitation"))
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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Source for this absolute altitude metadata."))
	EOpenMobileAbsoluteAltitudeSource Source =
		EOpenMobileAbsoluteAltitudeSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Altitude", meta = (ToolTip = "Whether Vertical Accuracy Meters contains the platform's estimated one-sided vertical uncertainty."))
	bool bHasVerticalAccuracy = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw vertical-accuracy backing value in metres. Read only when Has Vertical Accuracy is true."))
	double VerticalAccuracyMeters = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileScalarSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor identity, timing, validity, quality, and provenance shared by this sample."))
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Value", meta = (ToolTip = "Normalized scalar value. Pressure uses hectopascals, altitude uses metres, and ambient light uses lux."))
	double Value = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Altitude", meta = (AdvancedDisplay, ToolTip = "Source, baseline, and quality details used only by Relative Altitude samples."))
	FOpenMobileRelativeAltitudeMetadata RelativeAltitude;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Altitude", meta = (AdvancedDisplay, ToolTip = "Source and optional vertical accuracy used only by Absolute Altitude samples."))
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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor identity, timing, validity, quality, and provenance shared by this sample."))
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (ToolTip = "Heading in degrees clockwise from the reported north reference, normalized to the range from 0 inclusive to 360 exclusive."))
	double HeadingDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (ToolTip = "Whether Heading Degrees is referenced to magnetic north or true north."))
	EOpenMobileHeadingReference Reference =
		EOpenMobileHeadingReference::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (AdvancedDisplay, ToolTip = "Source of the magnetic-declination correction used for true heading."))
	EOpenMobileHeadingDeclinationSource DeclinationSource =
		EOpenMobileHeadingDeclinationSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (ToolTip = "Whether Declination Degrees contains a finite correction from magnetic north to true north."))
	bool bHasDeclinationDegrees = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (AdvancedDisplay, ToolTip = "Signed magnetic declination in degrees added to magnetic heading to obtain true heading. Read only when Has Declination Degrees is true."))
	double DeclinationDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (ToolTip = "Whether Location Age Seconds is available for the true-heading location input."))
	bool bHasLocationAgeSeconds = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (AdvancedDisplay, ToolTip = "Age in seconds of the authorized location fix used for true heading. Read only when Has Location Age Seconds is true."))
	double LocationAgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (ToolTip = "Whether the provider compensated heading for device pitch and roll."))
	bool bTiltCompensated = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (ToolTip = "Whether Accuracy Degrees contains a provider-reported angular uncertainty."))
	bool bHasAccuracyDegrees = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw heading-accuracy backing value in degrees. Read only when Has Accuracy Degrees is true."))
	double AccuracyDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Heading", meta = (ToolTip = "Whether magnetic interference or native quality indicates that compass calibration is needed."))
	bool bCalibrationRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Processing", meta = (ToolTip = "Whether shortest-arc exponential heading smoothing was enabled for this subscriber."))
	bool bExponentiallySmoothed = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Processing", meta = (ToolTip = "Whether the angular dead zone suppressed an output before this delivered heading."))
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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when distance meters is present and safe to read."))
	bool bHasDistanceMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw distance backing value in metres. Read only when Has Distance Meters is true."))
	double DistanceMeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when floors ascended is present and safe to read."))
	bool bHasFloorsAscended = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw ascending-floor count. Read only when Has Floors Ascended is true."))
	int64 FloorsAscended = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when floors descended is present and safe to read."))
	bool bHasFloorsDescended = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw descending-floor count. Read only when Has Floors Descended is true."))
	int64 FloorsDescended = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when pace seconds per meter is present and safe to read."))
	bool bHasPaceSecondsPerMeter = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Pace in seconds per metre for this pedometer metrics."))
	double PaceSecondsPerMeter = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "True when cadence steps per second is present and safe to read."))
	bool bHasCadenceStepsPerSecond = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Cadence in steps per second for this pedometer metrics."))
	double CadenceStepsPerSecond = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileStepsSensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor identity, timing, validity, quality, and provenance shared by this sample."))
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Step count relative to the reported Origin. Typed step listeners rebase the first cumulative total to zero for their session."))
	int64 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Zero point that gives Count its meaning, such as device boot, a query interval, or a listener session."))
	EOpenMobileStepCountOrigin Origin = EOpenMobileStepCountOrigin::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (AdvancedDisplay, ToolTip = "Stable identifier for the current count origin. A changed identifier marks a new baseline."))
	FGuid OriginIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Reason Count should not be differenced directly against the prior sample."))
	EOpenMobileStepCountDiscontinuity Discontinuity =
		EOpenMobileStepCountDiscontinuity::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Whether Count reached the maximum representable value and further increments cannot be represented."))
	bool bCountSaturated = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Number of newly detected steps represented by this StepDetector event."))
	int64 DetectedStepDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Whether Native Total contains the platform cumulative count used to derive this event."))
	bool bHasNativeTotal = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Platform cumulative total used to derive the event when available."))
	int64 NativeTotal = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Native or derived source that produced a discrete step event."))
	EOpenMobileStepDetectionSource DetectionSource =
		EOpenMobileStepDetectionSource::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Whether the step event came directly from hardware or was inferred from a pedometer delta."))
	EOpenMobileStepDetectionQuality DetectionQuality =
		EOpenMobileStepDetectionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Whether Query Start and End Unix Time Seconds describe this historical step result."))
	bool bHasQueryInterval = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Start of the native query interval in Unix time seconds."))
	double QueryStartUnixTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "End of the native query interval in Unix time seconds."))
	double QueryEndUnixTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Steps", meta = (ToolTip = "Optional distance, floors, pace, and cadence returned by a pedometer provider."))
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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor identity, timing, validity, quality, and provenance shared by this sample."))
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Activity", meta = (ToolTip = "Primary motion activity classified by the provider."))
	EOpenMobileMotionActivity Activity = EOpenMobileMotionActivity::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Activity", meta = (ToolTip = "Provider confidence in the primary activity classification."))
	EOpenMobileActivityConfidence Confidence =
		EOpenMobileActivityConfidence::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Activity", meta = (ToolTip = "Started or stopped transition represented by this sample, or None for a normal classification."))
	EOpenMobileActivityTransition Transition =
		EOpenMobileActivityTransition::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Activity", meta = (ToolTip = "Whether the transition was reported natively or derived from stable activity samples."))
	EOpenMobileActivityTransitionOrigin TransitionOrigin =
		EOpenMobileActivityTransitionOrigin::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Activity", meta = (AdvancedDisplay, ToolTip = "Stable name of the native, derived, mock, or replay activity provider."))
	FName ActivityProvider;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Activity", meta = (AdvancedDisplay, ToolTip = "Additional activities the provider reports concurrently with the primary classification."))
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

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor identity, timing, validity, quality, and provenance shared by this sample."))
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Orientation", meta = (ToolTip = "Classified physical device posture after configured hysteresis and debounce."))
	EOpenMobilePhysicalOrientation Orientation =
		EOpenMobilePhysicalOrientation::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Orientation", meta = (ToolTip = "Normalized classification confidence from 0 to 1."))
	double Confidence = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileProximitySensorSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor identity, timing, validity, quality, and provenance shared by this sample."))
	FOpenMobileSensorSampleHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Proximity", meta = (ToolTip = "Whether the provider currently classifies an object as near the sensor."))
	bool bNear = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Proximity", meta = (ToolTip = "Whether Distance Meters contains a measured proximity distance. Many mobile sensors report only Near or Far."))
	bool bHasDistanceMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw proximity-distance backing value in metres. Read only when Has Distance Meters is true."))
	double DistanceMeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Sample|Proximity", meta = (ToolTip = "Whether Maximum Range Meters contains the native sensor's documented measurable range."))
	bool bHasMaximumRangeMeters = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (AdvancedDisplay, ToolTip = "Raw proximity-range backing value in metres. Read only when Has Maximum Range Meters is true."))
	double MaximumRangeMeters = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileVectorSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples delivered in this batch in chronological order."))
	TArray<FOpenMobileVectorSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileAttitudeSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples delivered in this batch in chronological order."))
	TArray<FOpenMobileAttitudeSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileScalarSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples delivered in this batch in chronological order."))
	TArray<FOpenMobileScalarSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileHeadingSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples delivered in this batch in chronological order."))
	TArray<FOpenMobileHeadingSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileStepsSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples delivered in this batch in chronological order."))
	TArray<FOpenMobileStepsSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileActivitySensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples delivered in this batch in chronological order."))
	TArray<FOpenMobileActivitySensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileOrientationSensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples delivered in this batch in chronological order."))
	TArray<FOpenMobileOrientationSensorSample> Samples;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileProximitySensorBatch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Samples delivered in this batch in chronological order."))
	TArray<FOpenMobileProximitySensorSample> Samples;
};

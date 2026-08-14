#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorIdentifiers.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorType : uint8
{
	Unknown UMETA(DisplayName = "Unknown Sensor", ToolTip = "No recognized sensor type was selected or reported."),
	Accelerometer UMETA(DisplayName = "Accelerometer", ToolTip = "Measures acceleration including gravity in metres per second squared."),
	AccelerometerUncalibrated UMETA(DisplayName = "Accelerometer, Uncalibrated", ToolTip = "Measures raw acceleration and optional bias in metres per second squared."),
	Gyroscope UMETA(DisplayName = "Gyroscope", ToolTip = "Measures calibrated angular velocity in radians per second."),
	GyroscopeUncalibrated UMETA(DisplayName = "Gyroscope, Uncalibrated", ToolTip = "Measures raw angular velocity and optional bias in radians per second."),
	Magnetometer UMETA(DisplayName = "Magnetometer", ToolTip = "Measures calibrated magnetic field strength in microteslas."),
	MagnetometerUncalibrated UMETA(DisplayName = "Magnetometer, Uncalibrated", ToolTip = "Measures raw magnetic field and optional bias in microteslas."),
	Gravity UMETA(DisplayName = "Gravity", ToolTip = "Measures or estimates the gravity vector in metres per second squared."),
	LinearAcceleration UMETA(DisplayName = "Linear Acceleration", ToolTip = "Measures or estimates device acceleration with gravity removed, in metres per second squared."),
	Attitude UMETA(DisplayName = "Device Attitude", ToolTip = "Reports fused device orientation as a quaternion with requested optional representations."),
	MagneticHeading UMETA(DisplayName = "Magnetic Heading", ToolTip = "Reports compass heading in degrees clockwise from magnetic north."),
	TrueHeading UMETA(DisplayName = "True Heading", ToolTip = "Reports heading in degrees clockwise from true north using an authorized fresh location fix."),
	BarometricPressure UMETA(DisplayName = "Barometric Pressure", ToolTip = "Measures ambient atmospheric pressure in hectopascals."),
	RelativeAltitude UMETA(DisplayName = "Relative Altitude", ToolTip = "Reports vertical displacement in metres from a native or pressure baseline."),
	AbsoluteAltitude UMETA(DisplayName = "Absolute Altitude", ToolTip = "Reports platform-provided absolute altitude in metres."),
	AmbientLight UMETA(DisplayName = "Ambient Light", ToolTip = "Measures ambient illuminance in lux."),
	Proximity UMETA(DisplayName = "Proximity", ToolTip = "Reports near or far proximity and an optional distance in centimetres."),
	StepCounter UMETA(DisplayName = "Step Counter", ToolTip = "Reports a cumulative or session step count with an explicit count origin."),
	StepDetector UMETA(DisplayName = "Step Detector", ToolTip = "Reports discrete detected step events."),
	Pedometer UMETA(DisplayName = "Pedometer", ToolTip = "Reports step count with optional distance, pace, cadence, and floor metrics."),
	MotionActivity UMETA(DisplayName = "Motion Activity", ToolTip = "Classifies motion as stationary, walking, running, cycling, or automotive."),
	ActivityTransition UMETA(DisplayName = "Activity Transition", ToolTip = "Reports started or stopped transitions for classified motion activities."),
	PhysicalOrientation UMETA(DisplayName = "Physical Orientation", ToolTip = "Classifies device posture as portrait, landscape, face up, or face down."),
	Shake UMETA(DisplayName = "Shake Gesture", ToolTip = "Reports a derived shake gesture detected from motion input.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorSampleFamily : uint8
{
	Unknown UMETA(DisplayName = "Unknown Family", ToolTip = "The sensor does not map to a recognized public sample struct."),
	Vector UMETA(DisplayName = "Vector Sample", ToolTip = "Uses the vector sample family for acceleration, angular velocity, magnetic field, or gravity values."),
	Attitude UMETA(DisplayName = "Attitude Sample", ToolTip = "Uses the attitude sample family for fused device orientation."),
	Scalar UMETA(DisplayName = "Scalar Sample", ToolTip = "Uses the scalar sample family for pressure, altitude, or ambient light values."),
	Heading UMETA(DisplayName = "Heading Sample", ToolTip = "Uses the heading sample family for magnetic or true heading values."),
	Steps UMETA(DisplayName = "Steps Sample", ToolTip = "Uses a step count, step detection, or pedometer sample struct."),
	Activity UMETA(DisplayName = "Activity Sample", ToolTip = "Uses a motion activity or activity transition sample struct."),
	Orientation UMETA(DisplayName = "Orientation Sample", ToolTip = "Uses the physical orientation sample struct."),
	Proximity UMETA(DisplayName = "Proximity Sample", ToolTip = "Uses the proximity sample struct.")
};

class OPENMOBILESENSORS_API FOpenMobileSensorTypes final
{
public:
	/** You'll get every sensor type that can appear in the public contract. Unknown isn't included because it can't identify a stream. */
	static const TArray<EOpenMobileSensorType>& GetAll();

	/** Use this stable name for config, files, and logs. Display text can change, this name shouldn't. */
	static FName GetStableName(EOpenMobileSensorType Type);

	/** You'll get the typed sample family used by generic stream code. Unknown comes back when no public sample struct fits. */
	static EOpenMobileSensorSampleFamily GetSampleFamily(
		EOpenMobileSensorType Type
	);
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorIdentifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Type for this sensor identifier."))
	EOpenMobileSensorType Type = EOpenMobileSensorType::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Provider-defined sensor instance name. Default selects the normal device sensor."))
	FName InstanceId;

	/** Use this before a raw identifier reaches the subsystem. Unknown is the only invalid sensor type. */
	bool IsValid() const
	{
		return Type != EOpenMobileSensorType::Unknown;
	}

	/** This compares both sensor type and provider instance. Matching only the type could pick the wrong physical sensor. */
	bool operator==(const FOpenMobileSensorIdentifier& Other) const
	{
		return Type == Other.Type && InstanceId == Other.InstanceId;
	}

	/** This stays tied to the full equality check, so a new identity field can't be forgotten here. */
	bool operator!=(const FOpenMobileSensorIdentifier& Other) const
	{
		return !(*this == Other);
	}

	/** This hashes the same fields equality reads. You'll get safe map and set lookup for named sensor instances. */
	friend uint32 GetTypeHash(const FOpenMobileSensorIdentifier& Identifier)
	{
		return HashCombine(
			GetTypeHash(static_cast<uint8>(Identifier.Type)),
			GetTypeHash(Identifier.InstanceId)
		);
	}
};

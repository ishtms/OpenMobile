#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorIdentifiers.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorType : uint8
{
	Unknown,
	Accelerometer,
	AccelerometerUncalibrated,
	Gyroscope,
	GyroscopeUncalibrated,
	Magnetometer,
	MagnetometerUncalibrated,
	Gravity,
	LinearAcceleration,
	Attitude,
	MagneticHeading,
	TrueHeading,
	BarometricPressure,
	RelativeAltitude,
	AbsoluteAltitude,
	AmbientLight,
	Proximity,
	StepCounter,
	StepDetector,
	Pedometer,
	MotionActivity,
	ActivityTransition,
	PhysicalOrientation,
	Shake
};

UENUM(BlueprintType)
enum class EOpenMobileSensorSampleFamily : uint8
{
	Unknown,
	Vector,
	Attitude,
	Scalar,
	Heading,
	Steps,
	Activity,
	Orientation,
	Proximity
};

class OPENMOBILESENSORS_API FOpenMobileSensorTypes final
{
public:
	static const TArray<EOpenMobileSensorType>& GetAll();
	static FName GetStableName(EOpenMobileSensorType Type);
	static EOpenMobileSensorSampleFamily GetSampleFamily(
		EOpenMobileSensorType Type
	);
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorIdentifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	EOpenMobileSensorType Type = EOpenMobileSensorType::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors")
	FName InstanceId;

	bool IsValid() const
	{
		return Type != EOpenMobileSensorType::Unknown;
	}

	bool operator==(const FOpenMobileSensorIdentifier& Other) const
	{
		return Type == Other.Type && InstanceId == Other.InstanceId;
	}

	bool operator!=(const FOpenMobileSensorIdentifier& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FOpenMobileSensorIdentifier& Identifier)
	{
		return HashCombine(
			GetTypeHash(static_cast<uint8>(Identifier.Type)),
			GetTypeHash(Identifier.InstanceId)
		);
	}
};

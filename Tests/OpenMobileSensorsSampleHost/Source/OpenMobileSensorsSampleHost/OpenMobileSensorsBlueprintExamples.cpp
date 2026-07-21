#include "OpenMobileSensorsBlueprintExamples.h"

namespace OpenMobileSensorsBlueprintExamplesPrivate
{
	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetNameStringByValue(static_cast<int64>(Value))
			: TEXT("Unknown");
	}
}

FOpenMobileSensorSubscriptionRequest
UOpenMobileSensorsBlueprintExamples::MakeLowRateRequest(
	EOpenMobileSensorType SensorType,
	EOpenMobileSensorDeliveryMode DeliveryMode,
	EOpenMobileSensorCoordinateSpace CoordinateSpace
)
{
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = SensorType;
	Request.Options.RatePreset = EOpenMobileSensorRatePreset::UI;
	Request.Options.CustomFrequencyHz = 15.0;
	Request.Options.MaximumCallbackFrequencyHz = 10.0;
	Request.Options.MaximumDeliveryLatencySeconds = 0.05;
	Request.Options.DeliveryMode = DeliveryMode;
	Request.Options.CoordinateSpace = CoordinateSpace;
	Request.Options.BufferCapacitySamples = 128;
	Request.Options.bAllowHighSamplingRate = false;
	Request.Options.bLowLatency = false;
	if (SensorType == EOpenMobileSensorType::Attitude)
	{
		Request.Options.AttitudeRepresentations =
			static_cast<int32>(EOpenMobileAttitudeRepresentation::Quaternion)
			| static_cast<int32>(EOpenMobileAttitudeRepresentation::EulerAngles)
			| static_cast<int32>(EOpenMobileAttitudeRepresentation::RotationMatrix);
	}
	return Request;
}

FVector2D UOpenMobileSensorsBlueprintExamples::ComputeTiltSteering(
	const FVector& AccelerationMetresPerSecondSquared
)
{
	constexpr double StandardGravity = 9.80665;
	return FVector2D(
		FMath::Clamp(
			AccelerationMetresPerSecondSquared.Y / StandardGravity,
			-1.0,
			1.0
		),
		FMath::Clamp(
			-AccelerationMetresPerSecondSquared.X / StandardGravity,
			-1.0,
			1.0
		)
	);
}

FVector2D UOpenMobileSensorsBlueprintExamples::ComputeGyroAimDelta(
	const FVector& AngularVelocityRadiansPerSecond,
	double DeltaSeconds
)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
	{
		return FVector2D::ZeroVector;
	}
	return FVector2D(
		FMath::RadiansToDegrees(
			AngularVelocityRadiansPerSecond.Z * DeltaSeconds
		),
		FMath::RadiansToDegrees(
			AngularVelocityRadiansPerSecond.Y * DeltaSeconds
		)
	);
}

FString UOpenMobileSensorsBlueprintExamples::DescribeSampleHeader(
	const FOpenMobileSensorSampleHeader& Header,
	double SampleAgeSeconds
)
{
	using namespace OpenMobileSensorsBlueprintExamplesPrivate;
	return FString::Printf(
		TEXT("age %.3fs | quality %s | source 0x%X | %s | rotation %s"),
		FMath::Max(0.0, SampleAgeSeconds),
		*EnumName(Header.Accuracy),
		Header.SourceFlags,
		Header.bValid ? TEXT("valid") : TEXT("invalid"),
		*EnumName(Header.ScreenRotation)
	);
}

FText UOpenMobileSensorsBlueprintExamples::GetAxisConvention()
{
	return NSLOCTEXT(
		"OpenMobileSensorsSample",
		"AxisConvention",
		"Unreal device axes: +X forward toward the top edge, +Y right, +Z out of the screen. Attitude basis rows show forward, right, and up after native normalization. Current-screen streams rotate only when the app supplies a timestamped window rotation."
	);
}

FText UOpenMobileSensorsBlueprintExamples::GetBlueprintRecipes()
{
	return NSLOCTEXT(
		"OpenMobileSensorsSample",
		"BlueprintRecipes",
		"Blueprint recipes\nTilt steering: Make Low Rate Request for Accelerometer, poll Get Latest Vector Sample, then Compute Tilt Steering.\nGyro aiming: poll Gyroscope and pass its value plus frame delta to Compute Gyro Aim Delta.\nCompass: subscribe to Magnetic Heading and read Heading Degrees with accuracy.\nAttitude: request Euler and matrix extras, then call Recenter Sensor Attitude for yaw-only or full recenter.\nSteps and altitude: begin owner-scoped sessions, poll, reset or recenter, then stop the returned handle.\nAlways stop owned handles during Destruct."
	);
}

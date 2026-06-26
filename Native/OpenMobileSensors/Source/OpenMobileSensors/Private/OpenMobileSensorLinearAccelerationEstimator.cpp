#include "OpenMobileSensorLinearAccelerationEstimator.h"

#include "OpenMobileSensorFusionQuality.h"
#include "OpenMobileSensorSourcePolicy.h"

namespace OpenMobileSensorLinearAccelerationEstimatorPrivate
{
	constexpr double EstimatedLagSeconds = 0.5;

	int32 DerivedSourceFlags(int32 InputFlags)
	{
		constexpr int32 Overlays =
			static_cast<int32>(EOpenMobileSensorSourceFlags::Mock)
			| static_cast<int32>(EOpenMobileSensorSourceFlags::Replay);
		return static_cast<int32>(
			EOpenMobileSensorSourceFlags::PluginDerived
		) | (InputFlags & Overlays);
	}

	bool IsStructurallyValid(
		const FOpenMobileVectorSensorSample& Acceleration
	)
	{
		return Acceleration.Header.Sensor.Type ==
				EOpenMobileSensorType::Accelerometer
			&& Acceleration.Header.bUnitsNormalized
			&& Acceleration.Header.bCoordinatesNormalized
			&& FMath::IsFinite(Acceleration.Header.TimestampSeconds)
			&& Acceleration.Header.TimestampSeconds >= 0.0
			&& !Acceleration.Value.ContainsNaN()
			&& FOpenMobileSensorSourcePolicy::ValidateSourceFlags(
				Acceleration.Header.SourceFlags
			);
	}

	bool IsSufficientInput(
		const FOpenMobileVectorSensorSample& Acceleration
	)
	{
		return Acceleration.Header.bValid
			&& Acceleration.Header.Accuracy !=
				EOpenMobileSensorAccuracy::Unreliable
			&& Acceleration.Header.Accuracy !=
				EOpenMobileSensorAccuracy::Low
			&& !Acceleration.Header.bCalibrationRequired
			&& (Acceleration.Header.SourceFlags
				& static_cast<int32>(
					EOpenMobileSensorSourceFlags::CalibratedNative
				)) != 0;
	}

	FOpenMobileSensorFusionContext MakeInvalidFusion(
		const FOpenMobileVectorSensorSample& Acceleration
	)
	{
		FOpenMobileSensorFusionInputObservation Input;
		Input.Sensor = EOpenMobileSensorType::Accelerometer;
		Input.bExpected = true;
		Input.bAvailable = true;
		Input.bValid = false;
		Input.bContributed = false;
		Input.Accuracy = Acceleration.Header.Accuracy;
		Input.bCalibrationRequired =
			Acceleration.Header.bCalibrationRequired;
		FOpenMobileSensorFusionContext Fusion =
			FOpenMobileSensorFusionQualityEvaluator::Evaluate(
				MakeArrayView(&Input, 1)
			);
		Fusion.bHasEstimatedLag = true;
		Fusion.EstimatedLagSeconds = EstimatedLagSeconds;
		return Fusion;
	}

	void PrepareOutput(
		const FOpenMobileVectorSensorSample& Acceleration,
		const FOpenMobileSensorIdentifier& LinearAccelerationSensor,
		FOpenMobileVectorSensorSample& OutLinearAcceleration
	)
	{
		OutLinearAcceleration = Acceleration;
		OutLinearAcceleration.Header.Sensor = LinearAccelerationSensor;
		OutLinearAcceleration.Header.SourceFlags = DerivedSourceFlags(
			Acceleration.Header.SourceFlags
		);
		OutLinearAcceleration.Header.bSourceChanged = false;
		OutLinearAcceleration.Value = FVector::ZeroVector;
		OutLinearAcceleration.bHasBias = false;
		OutLinearAcceleration.Bias = FVector::ZeroVector;
	}
}

bool FOpenMobileSensorLinearAccelerationEstimator::Process(
	const FOpenMobileVectorSensorSample& Acceleration,
	const FOpenMobileSensorIdentifier& LinearAccelerationSensor,
	FOpenMobileVectorSensorSample& OutLinearAcceleration
)
{
	using namespace OpenMobileSensorLinearAccelerationEstimatorPrivate;
	OutLinearAcceleration = {};
	if (!LinearAccelerationSensor.IsValid()
		|| LinearAccelerationSensor.Type !=
			EOpenMobileSensorType::LinearAcceleration
		|| !IsStructurallyValid(Acceleration))
	{
		Reset();
		return false;
	}
	PrepareOutput(
		Acceleration,
		LinearAccelerationSensor,
		OutLinearAcceleration
	);
	if (!IsSufficientInput(Acceleration))
	{
		Reset();
		OutLinearAcceleration.Header.bValid = false;
		OutLinearAcceleration.Header.bStatefulProcessingReset = true;
		OutLinearAcceleration.Header.Fusion = MakeInvalidFusion(Acceleration);
		return true;
	}
	FOpenMobileVectorSensorSample Gravity;
	FOpenMobileSensorIdentifier GravitySensor;
	GravitySensor.Type = EOpenMobileSensorType::Gravity;
	GravitySensor.InstanceId = LinearAccelerationSensor.InstanceId;
	if (!GravityEstimator.Process(
		Acceleration,
		GravitySensor,
		Gravity
	))
	{
		Reset();
		OutLinearAcceleration.Header.bValid = false;
		OutLinearAcceleration.Header.bStatefulProcessingReset = true;
		OutLinearAcceleration.Header.Fusion = MakeInvalidFusion(Acceleration);
		return true;
	}
	OutLinearAcceleration.Header.Fusion = Gravity.Header.Fusion;
	OutLinearAcceleration.Header.Fusion.Quality =
		EOpenMobileSensorFusionQuality::Degraded;
	OutLinearAcceleration.Header.Fusion.bHasEstimatedLag = true;
	OutLinearAcceleration.Header.Fusion.EstimatedLagSeconds =
		EstimatedLagSeconds;
	OutLinearAcceleration.Header.bStatefulProcessingReset |=
		Gravity.Header.bStatefulProcessingReset;
	OutLinearAcceleration.Value = Acceleration.Value - Gravity.Value;
	return true;
}

void FOpenMobileSensorLinearAccelerationEstimator::Reset()
{
	GravityEstimator.Reset();
}

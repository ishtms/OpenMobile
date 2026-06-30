#include "OpenMobileSensorGravityEstimator.h"

#include "OpenMobileSensorFusionQuality.h"

namespace OpenMobileSensorGravityEstimatorPrivate
{
	constexpr double StandardGravity = 9.80665;
	constexpr double TimeConstantSeconds = 0.5;
	constexpr double MaximumGapSeconds = 0.5;
	constexpr double ContaminationThreshold = 2.0;

	bool IsSufficientInput(const FOpenMobileVectorSensorSample& Sample)
	{
		return Sample.Header.Sensor.Type ==
				EOpenMobileSensorType::Accelerometer
			&& Sample.Header.bValid
			&& Sample.Header.bUnitsNormalized
			&& Sample.Header.bCoordinatesNormalized
			&& FMath::IsFinite(Sample.Header.TimestampSeconds)
			&& Sample.Header.TimestampSeconds >= 0.0
			&& !Sample.Value.ContainsNaN()
			&& Sample.Header.Accuracy !=
				EOpenMobileSensorAccuracy::Unreliable
			&& Sample.Header.Accuracy != EOpenMobileSensorAccuracy::Low
			&& !Sample.Header.bCalibrationRequired;
	}

	int32 DerivedSourceFlags(int32 InputFlags)
	{
		constexpr int32 Overlays =
			static_cast<int32>(EOpenMobileSensorSourceFlags::Mock)
			| static_cast<int32>(EOpenMobileSensorSourceFlags::Replay);
		return static_cast<int32>(
			EOpenMobileSensorSourceFlags::PluginDerived
		) | (InputFlags & Overlays);
	}

	FOpenMobileSensorFusionContext MakeFusion(
		const FOpenMobileVectorSensorSample& Acceleration,
		bool bContaminated
	)
	{
		FOpenMobileSensorFusionInputObservation Input;
		Input.Sensor = EOpenMobileSensorType::Accelerometer;
		Input.bExpected = true;
		Input.bAvailable = true;
		Input.bValid = true;
		Input.bContributed = true;
		Input.Accuracy = Acceleration.Header.Accuracy;
		Input.bCalibrationRequired =
			Acceleration.Header.bCalibrationRequired;
		FOpenMobileSensorFusionContext Fusion =
			FOpenMobileSensorFusionQualityEvaluator::Evaluate(
				MakeArrayView(&Input, 1)
			);
		if (bContaminated)
		{
			Fusion.DegradedInputMask |=
				UOpenMobileSensorQualityLibrary::MakeInputMask(
					EOpenMobileSensorType::Accelerometer
				);
			Fusion.Quality = EOpenMobileSensorFusionQuality::Degraded;
		}
		return Fusion;
	}
}

bool FOpenMobileSensorGravityEstimator::Process(
	const FOpenMobileVectorSensorSample& Acceleration,
	const FOpenMobileSensorIdentifier& GravitySensor,
	FOpenMobileVectorSensorSample& OutGravity
)
{
	using namespace OpenMobileSensorGravityEstimatorPrivate;
	OutGravity = {};
	if (!GravitySensor.IsValid()
		|| GravitySensor.Type != EOpenMobileSensorType::Gravity
		|| !IsSufficientInput(Acceleration))
	{
		Reset();
		return false;
	}
	const bool bInputSourceChanged = bHasEstimate
		&& Acceleration.Header.SourceFlags != LastSourceFlags;
	bool bReset = Acceleration.Header.bStatefulProcessingReset
		|| !bHasEstimate
		|| bInputSourceChanged;
	double DeltaSeconds = 0.0;
	if (bHasEstimate && !bReset)
	{
		DeltaSeconds = Acceleration.Header.TimestampSeconds -
			LastTimestampSeconds;
		if (!FMath::IsFinite(DeltaSeconds)
			|| DeltaSeconds <= 0.0)
		{
			Reset();
			return false;
		}
		bReset = DeltaSeconds > MaximumGapSeconds;
	}
	if (bReset)
	{
		Estimate = Acceleration.Value;
	}
	else
	{
		const double Alpha = 1.0 - FMath::Exp(
			-DeltaSeconds / TimeConstantSeconds
		);
		Estimate += (Acceleration.Value - Estimate) * Alpha;
	}
	bHasEstimate = true;
	LastTimestampSeconds = Acceleration.Header.TimestampSeconds;
	LastSourceFlags = Acceleration.Header.SourceFlags;
	const bool bContaminated = FMath::Abs(
		Acceleration.Value.Size() - StandardGravity
	) > ContaminationThreshold;
	OutGravity = Acceleration;
	OutGravity.Header.Sensor = GravitySensor;
	OutGravity.Header.SourceFlags = DerivedSourceFlags(
		Acceleration.Header.SourceFlags
	);
	OutGravity.Header.bSourceChanged |= bInputSourceChanged;
	OutGravity.Header.bStatefulProcessingReset |= bReset;
	OutGravity.Header.Fusion = MakeFusion(Acceleration, bContaminated);
	OutGravity.Value = Estimate;
	OutGravity.bHasBias = false;
	OutGravity.Bias = FVector::ZeroVector;
	return true;
}

void FOpenMobileSensorGravityEstimator::Reset()
{
	Estimate = FVector::ZeroVector;
	LastTimestampSeconds = 0.0;
	LastSourceFlags = 0;
	bHasEstimate = false;
}

#include "OpenMobileSensorUnits.h"

namespace OpenMobileSensorUnitsPrivate
{
	constexpr double StandardGravityMetersPerSecondSquared = 9.80665;
	constexpr double CentimetersToMeters = 0.01;
	constexpr double KilopascalsToHectopascals = 10.0;

	void NormalizeAccuracyMetadata(FOpenMobileSensorSampleHeader& Header)
	{
		if (Header.bHasEstimatedError
			&& (!FMath::IsFinite(Header.EstimatedError)
				|| Header.EstimatedError < 0.0))
		{
			Header.bHasEstimatedError = false;
		}
	}

	void MarkNormalized(FOpenMobileSensorSampleHeader& Header, bool bValid)
	{
		NormalizeAccuracyMetadata(Header);
		Header.bUnitsNormalized = true;
		Header.bValid &= bValid;
	}

	void CaptureDiagnostics(
		FOpenMobileSensorNativeUnitDiagnostics* Diagnostics,
		std::initializer_list<double> Values
	)
	{
#if !UE_BUILD_SHIPPING
		if (!Diagnostics)
		{
			return;
		}
		Diagnostics->SanitizedValueCount = FMath::Min(
			static_cast<int32>(Values.size()),
			static_cast<int32>(UE_ARRAY_COUNT(Diagnostics->SanitizedValues))
		);
		int32 Index = 0;
		for (const double Value : Values)
		{
			if (Index >= Diagnostics->SanitizedValueCount)
			{
				break;
			}
			Diagnostics->SanitizedValues[Index++] =
				FOpenMobileSensorUnitConverter::SanitizeNativeValue(Value);
		}
#else
		static_cast<void>(Diagnostics);
		static_cast<void>(Values);
#endif
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return !Value.ContainsNaN();
	}

	bool IsFiniteNonnegative(double Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0;
	}
}

double FOpenMobileSensorUnitConverter::SanitizeNativeValue(double Value)
{
	return FMath::IsFinite(Value)
		? FMath::Clamp(Value, -1.e12, 1.e12)
		: 0.0;
}

bool FOpenMobileSensorUnitConverter::NormalizeVectorSample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileVectorSensorSample& Sample,
	FOpenMobileSensorNativeUnitDiagnostics* Diagnostics
)
{
	using namespace OpenMobileSensorUnitsPrivate;
	if (Sample.Header.bUnitsNormalized)
	{
		return true;
	}
	CaptureDiagnostics(
		Diagnostics,
		{Sample.Value.X, Sample.Value.Y, Sample.Value.Z}
	);
	bool bValid = IsFiniteVector(Sample.Value)
		&& (!Sample.bHasBias || IsFiniteVector(Sample.Bias));
	const EOpenMobileSensorType SensorType = Sample.Header.Sensor.Type;
	const bool bAcceleration =
		SensorType == EOpenMobileSensorType::Accelerometer
		|| SensorType == EOpenMobileSensorType::AccelerometerUncalibrated
		|| SensorType == EOpenMobileSensorType::Gravity
		|| SensorType == EOpenMobileSensorType::LinearAcceleration;
	if (bValid
		&& Platform == EOpenMobileSensorNativePlatform::IOS
		&& bAcceleration)
	{
		Sample.Value *= StandardGravityMetersPerSecondSquared;
		if (Sample.bHasBias)
		{
			Sample.Bias *= StandardGravityMetersPerSecondSquared;
		}
	}
	MarkNormalized(Sample.Header, bValid);
	return bValid;
}

bool FOpenMobileSensorUnitConverter::NormalizeAttitudeSample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileAttitudeSensorSample& Sample,
	FOpenMobileSensorNativeUnitDiagnostics* Diagnostics
)
{
	using namespace OpenMobileSensorUnitsPrivate;
	static_cast<void>(Platform);
	if (Sample.Header.bUnitsNormalized)
	{
		return true;
	}
	CaptureDiagnostics(
		Diagnostics,
		{
			Sample.Quaternion.X,
			Sample.Quaternion.Y,
			Sample.Quaternion.Z,
			Sample.Quaternion.W
		}
	);
	bool bValid = !Sample.Quaternion.ContainsNaN()
		&& Sample.Quaternion.SizeSquared() > UE_DOUBLE_SMALL_NUMBER;
	if (bValid)
	{
		Sample.Quaternion.Normalize();
		if (Sample.bHasEulerDegrees)
		{
			Sample.EulerDegrees.Pitch = FRotator::NormalizeAxis(
				Sample.EulerDegrees.Pitch
			);
			Sample.EulerDegrees.Yaw = FRotator::NormalizeAxis(
				Sample.EulerDegrees.Yaw
			);
			Sample.EulerDegrees.Roll = FRotator::NormalizeAxis(
				Sample.EulerDegrees.Roll
			);
			bValid = !Sample.EulerDegrees.ContainsNaN();
		}
		if (Sample.bHasRotationMatrix)
		{
			bValid &= IsFiniteVector(Sample.RotationMatrix.XAxis)
				&& IsFiniteVector(Sample.RotationMatrix.YAxis)
				&& IsFiniteVector(Sample.RotationMatrix.ZAxis);
		}
	}
	MarkNormalized(Sample.Header, bValid);
	return bValid;
}

bool FOpenMobileSensorUnitConverter::NormalizeScalarSample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileScalarSensorSample& Sample,
	FOpenMobileSensorNativeUnitDiagnostics* Diagnostics
)
{
	using namespace OpenMobileSensorUnitsPrivate;
	if (Sample.Header.bUnitsNormalized)
	{
		return true;
	}
	CaptureDiagnostics(Diagnostics, {Sample.Value});
	bool bValid = FMath::IsFinite(Sample.Value);
	if (bValid
		&& Platform == EOpenMobileSensorNativePlatform::IOS
		&& Sample.Header.Sensor.Type ==
			EOpenMobileSensorType::BarometricPressure)
	{
		Sample.Value *= KilopascalsToHectopascals;
	}
	if (Sample.Header.Sensor.Type ==
		EOpenMobileSensorType::BarometricPressure)
	{
		bValid = bValid
			&& Sample.Value > 0.0
			&& Sample.Value <= 2000.0;
	}
	MarkNormalized(Sample.Header, bValid);
	return bValid;
}

bool FOpenMobileSensorUnitConverter::NormalizeHeadingSample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileHeadingSensorSample& Sample,
	FOpenMobileSensorNativeUnitDiagnostics* Diagnostics
)
{
	using namespace OpenMobileSensorUnitsPrivate;
	static_cast<void>(Platform);
	if (Sample.Header.bUnitsNormalized)
	{
		return true;
	}
	CaptureDiagnostics(
		Diagnostics,
		{Sample.HeadingDegrees, Sample.AccuracyDegrees}
	);
	const bool bValid = FMath::IsFinite(Sample.HeadingDegrees);
	if (bValid)
	{
		Sample.HeadingDegrees = FMath::Fmod(Sample.HeadingDegrees, 360.0);
		if (Sample.HeadingDegrees < 0.0)
		{
			Sample.HeadingDegrees += 360.0;
		}
	}
	if (Sample.bHasAccuracyDegrees
		&& !IsFiniteNonnegative(Sample.AccuracyDegrees))
	{
		Sample.bHasAccuracyDegrees = false;
	}
	else if (Sample.bHasAccuracyDegrees)
	{
		Sample.Header.bHasEstimatedError = true;
		Sample.Header.EstimatedError = Sample.AccuracyDegrees;
	}
	Sample.Header.bCalibrationRequired |= Sample.bCalibrationRequired;
	MarkNormalized(Sample.Header, bValid);
	return bValid;
}

bool FOpenMobileSensorUnitConverter::NormalizeStepsSample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileStepsSensorSample& Sample,
	FOpenMobileSensorNativeUnitDiagnostics* Diagnostics
)
{
	using namespace OpenMobileSensorUnitsPrivate;
	static_cast<void>(Platform);
	if (Sample.Header.bUnitsNormalized)
	{
		return true;
	}
	CaptureDiagnostics(Diagnostics, {static_cast<double>(Sample.Count)});
	bool bValid = Sample.Count >= 0;
	if (Sample.Metrics.bHasDistanceMeters)
	{
		bValid &= IsFiniteNonnegative(Sample.Metrics.DistanceMeters);
	}
	if (Sample.Metrics.bHasPaceSecondsPerMeter)
	{
		bValid &= IsFiniteNonnegative(
			Sample.Metrics.PaceSecondsPerMeter
		);
	}
	if (Sample.Metrics.bHasCadenceStepsPerSecond)
	{
		bValid &= IsFiniteNonnegative(
			Sample.Metrics.CadenceStepsPerSecond
		);
	}
	MarkNormalized(Sample.Header, bValid);
	return bValid;
}

bool FOpenMobileSensorUnitConverter::NormalizeActivitySample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileActivitySensorSample& Sample,
	FOpenMobileSensorNativeUnitDiagnostics* Diagnostics
)
{
	using namespace OpenMobileSensorUnitsPrivate;
	static_cast<void>(Platform);
	if (Sample.Header.bUnitsNormalized)
	{
		return true;
	}
	CaptureDiagnostics(Diagnostics, {});
	MarkNormalized(Sample.Header, true);
	return true;
}

bool FOpenMobileSensorUnitConverter::NormalizeOrientationSample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileOrientationSensorSample& Sample,
	FOpenMobileSensorNativeUnitDiagnostics* Diagnostics
)
{
	using namespace OpenMobileSensorUnitsPrivate;
	static_cast<void>(Platform);
	if (Sample.Header.bUnitsNormalized)
	{
		return true;
	}
	CaptureDiagnostics(Diagnostics, {Sample.Confidence});
	const bool bValid = FMath::IsFinite(Sample.Confidence);
	if (bValid)
	{
		Sample.Confidence = FMath::Clamp(Sample.Confidence, 0.0, 1.0);
	}
	MarkNormalized(Sample.Header, bValid);
	return bValid;
}

bool FOpenMobileSensorUnitConverter::NormalizeProximitySample(
	EOpenMobileSensorNativePlatform Platform,
	FOpenMobileProximitySensorSample& Sample,
	FOpenMobileSensorNativeUnitDiagnostics* Diagnostics
)
{
	using namespace OpenMobileSensorUnitsPrivate;
	if (Sample.Header.bUnitsNormalized)
	{
		return true;
	}
	CaptureDiagnostics(
		Diagnostics,
		{Sample.DistanceMeters, Sample.MaximumRangeMeters}
	);
	bool bValid = true;
	if (Sample.bHasDistanceMeters)
	{
		bValid &= IsFiniteNonnegative(Sample.DistanceMeters);
		if (Platform == EOpenMobileSensorNativePlatform::Android)
		{
			Sample.DistanceMeters *= CentimetersToMeters;
		}
	}
	if (Sample.bHasMaximumRangeMeters)
	{
		bValid &= IsFiniteNonnegative(Sample.MaximumRangeMeters);
		if (Platform == EOpenMobileSensorNativePlatform::Android)
		{
			Sample.MaximumRangeMeters *= CentimetersToMeters;
		}
	}
	MarkNormalized(Sample.Header, bValid);
	return bValid;
}

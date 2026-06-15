#include "OpenMobileSensorValidity.h"

namespace OpenMobileSensorValidityPrivate
{
	bool IsFiniteVector(const FVector& Value)
	{
		return !Value.ContainsNaN();
	}

	bool IsFiniteNonnegative(double Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0;
	}
}

bool FOpenMobileSensorValidity::IsEligibleForStatefulProcessing(
	const FOpenMobileVectorSensorSample& Sample
)
{
	return Sample.Header.bValid
		&& OpenMobileSensorValidityPrivate::IsFiniteVector(Sample.Value)
		&& (!Sample.bHasBias
			|| OpenMobileSensorValidityPrivate::IsFiniteVector(Sample.Bias));
}

bool FOpenMobileSensorValidity::IsEligibleForStatefulProcessing(
	const FOpenMobileAttitudeSensorSample& Sample
)
{
	return Sample.Header.bValid
		&& !Sample.Quaternion.ContainsNaN()
		&& Sample.Quaternion.SizeSquared() > UE_DOUBLE_SMALL_NUMBER
		&& (!Sample.bHasEulerDegrees || !Sample.EulerDegrees.ContainsNaN())
		&& (!Sample.bHasRotationMatrix
			|| (OpenMobileSensorValidityPrivate::IsFiniteVector(
					Sample.RotationMatrix.XAxis)
				&& OpenMobileSensorValidityPrivate::IsFiniteVector(
					Sample.RotationMatrix.YAxis)
				&& OpenMobileSensorValidityPrivate::IsFiniteVector(
					Sample.RotationMatrix.ZAxis)));
}

bool FOpenMobileSensorValidity::IsEligibleForStatefulProcessing(
	const FOpenMobileScalarSensorSample& Sample
)
{
	return Sample.Header.bValid && FMath::IsFinite(Sample.Value);
}

bool FOpenMobileSensorValidity::IsEligibleForStatefulProcessing(
	const FOpenMobileHeadingSensorSample& Sample
)
{
	return Sample.Header.bValid && FMath::IsFinite(Sample.HeadingDegrees);
}

bool FOpenMobileSensorValidity::IsEligibleForStatefulProcessing(
	const FOpenMobileStepsSensorSample& Sample
)
{
	return Sample.Header.bValid
		&& Sample.Count >= 0
		&& (!Sample.Metrics.bHasDistanceMeters
			|| OpenMobileSensorValidityPrivate::IsFiniteNonnegative(
				Sample.Metrics.DistanceMeters))
		&& (!Sample.Metrics.bHasFloorsAscended
			|| Sample.Metrics.FloorsAscended >= 0)
		&& (!Sample.Metrics.bHasFloorsDescended
			|| Sample.Metrics.FloorsDescended >= 0)
		&& (!Sample.Metrics.bHasPaceSecondsPerMeter
			|| OpenMobileSensorValidityPrivate::IsFiniteNonnegative(
				Sample.Metrics.PaceSecondsPerMeter))
		&& (!Sample.Metrics.bHasCadenceStepsPerSecond
			|| OpenMobileSensorValidityPrivate::IsFiniteNonnegative(
				Sample.Metrics.CadenceStepsPerSecond));
}

bool FOpenMobileSensorValidity::IsEligibleForStatefulProcessing(
	const FOpenMobileActivitySensorSample& Sample
)
{
	return Sample.Header.bValid;
}

bool FOpenMobileSensorValidity::IsEligibleForStatefulProcessing(
	const FOpenMobileOrientationSensorSample& Sample
)
{
	return Sample.Header.bValid && FMath::IsFinite(Sample.Confidence);
}

bool FOpenMobileSensorValidity::IsEligibleForStatefulProcessing(
	const FOpenMobileProximitySensorSample& Sample
)
{
	return Sample.Header.bValid
		&& (!Sample.bHasDistanceMeters
			|| OpenMobileSensorValidityPrivate::IsFiniteNonnegative(
				Sample.DistanceMeters))
		&& (!Sample.bHasMaximumRangeMeters
			|| OpenMobileSensorValidityPrivate::IsFiniteNonnegative(
				Sample.MaximumRangeMeters));
}

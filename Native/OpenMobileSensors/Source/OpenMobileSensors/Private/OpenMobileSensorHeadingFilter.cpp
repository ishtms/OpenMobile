#include "OpenMobileSensorHeadingFilter.h"

#include "OpenMobileSensorStreamOptions.h"

namespace OpenMobileSensorHeadingFilterPrivate
{
	double NormalizeHeading(double HeadingDegrees)
	{
		double Normalized = FMath::Fmod(HeadingDegrees, 360.0);
		if (Normalized < 0.0)
		{
			Normalized += 360.0;
		}
		return Normalized;
	}
}

bool FOpenMobileSensorHeadingFilter::Apply(
	const FOpenMobileSensorFilterOptions& Options,
	FOpenMobileHeadingSensorSample& Sample
)
{
	using namespace OpenMobileSensorHeadingFilterPrivate;
	if (!Sample.Header.bValid
		|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
		|| Sample.Header.TimestampSeconds < 0.0
		|| !FMath::IsFinite(Sample.HeadingDegrees)
		|| (Options.bEnableExponentialSmoothing
			&& (!FMath::IsFinite(Options.SmoothingTimeConstantSeconds)
				|| Options.SmoothingTimeConstantSeconds <= 0.0))
		|| !FMath::IsFinite(Options.DeadZone)
		|| Options.DeadZone < 0.0)
	{
		Reset();
		return false;
	}
	Sample.bExponentiallySmoothed =
		Options.bEnableExponentialSmoothing;
	Sample.bDeadZoneSuppressed = false;
	const bool bReset = Sample.Header.bStatefulProcessingReset
		|| !bInitialized
		|| Sample.Header.TimestampSeconds <= LastTimestampSeconds;
	double Value = NormalizeHeading(Sample.HeadingDegrees);
	if (bReset)
	{
		SmoothedHeadingDegrees = Value;
		bInitialized = true;
	}
	else if (Options.bEnableExponentialSmoothing)
	{
		const double DeltaSeconds =
			Sample.Header.TimestampSeconds - LastTimestampSeconds;
		const double Alpha = DeltaSeconds
			/ (Options.SmoothingTimeConstantSeconds + DeltaSeconds);
		const double DeltaDegrees = FMath::FindDeltaAngleDegrees(
			SmoothedHeadingDegrees,
			Value
		);
		SmoothedHeadingDegrees = NormalizeHeading(
			SmoothedHeadingDegrees + Alpha * DeltaDegrees
		);
		Value = SmoothedHeadingDegrees;
	}
	const double DistanceFromNorth = FMath::Abs(
		FMath::FindDeltaAngleDegrees(0.0, Value)
	);
	if (Options.DeadZone > 0.0
		&& DistanceFromNorth > 0.0
		&& DistanceFromNorth <= Options.DeadZone)
	{
		Value = 0.0;
		Sample.bDeadZoneSuppressed = true;
	}
	Sample.HeadingDegrees = Value;
	LastTimestampSeconds = Sample.Header.TimestampSeconds;
	return true;
}

void FOpenMobileSensorHeadingFilter::Reset()
{
	SmoothedHeadingDegrees = 0.0;
	LastTimestampSeconds = 0.0;
	bInitialized = false;
}

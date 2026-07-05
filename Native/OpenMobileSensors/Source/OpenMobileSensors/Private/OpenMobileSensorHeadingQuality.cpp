#include "OpenMobileSensorHeadingQuality.h"

namespace OpenMobileSensorHeadingQualityPrivate
{
EOpenMobileSensorAccuracy CapQuality(
	EOpenMobileSensorAccuracy Quality,
	EOpenMobileSensorAccuracy Maximum
)
{
	if (Quality == EOpenMobileSensorAccuracy::Unknown)
	{
		return EOpenMobileSensorAccuracy::Unknown;
	}
	return static_cast<uint8>(Quality) > static_cast<uint8>(Maximum)
		? Maximum
		: Quality;
}
}

void FOpenMobileSensorHeadingQuality::Normalize(
	FOpenMobileHeadingSensorSample& Sample
)
{
	using namespace OpenMobileSensorHeadingQualityPrivate;
	if (Sample.bHasAccuracyDegrees)
	{
		if (FMath::IsFinite(Sample.AccuracyDegrees)
			&& Sample.AccuracyDegrees >= 0.0)
		{
			Sample.Header.bHasEstimatedError = true;
			Sample.Header.EstimatedError = Sample.AccuracyDegrees;
		}
		else
		{
			Sample.bHasAccuracyDegrees = false;
		}
	}
	if (Sample.Header.bHasEstimatedError)
	{
		if (FMath::IsFinite(Sample.Header.EstimatedError)
			&& Sample.Header.EstimatedError >= 0.0)
		{
			Sample.bHasAccuracyDegrees = true;
			Sample.AccuracyDegrees = Sample.Header.EstimatedError;
		}
		else
		{
			Sample.Header.bHasEstimatedError = false;
			Sample.bHasAccuracyDegrees = false;
		}
	}
	Sample.Header.bCalibrationRequired |= Sample.bCalibrationRequired;
	Sample.bCalibrationRequired = Sample.Header.bCalibrationRequired;
	if (Sample.Header.bCalibrationRequired)
	{
		Sample.Header.Accuracy = EOpenMobileSensorAccuracy::Unreliable;
		return;
	}
	if (Sample.Header.Fusion.Quality ==
		EOpenMobileSensorFusionQuality::Degraded)
	{
		Sample.Header.Accuracy = CapQuality(
			Sample.Header.Accuracy,
			EOpenMobileSensorAccuracy::Low
		);
	}
	if (Sample.Header.Sensor.Type != EOpenMobileSensorType::TrueHeading)
	{
		return;
	}
	if (!Sample.bHasLocationAgeSeconds
		|| !FMath::IsFinite(Sample.LocationAgeSeconds)
		|| Sample.LocationAgeSeconds < 0.0
		|| Sample.LocationAgeSeconds > 60.0)
	{
		Sample.Header.Accuracy = EOpenMobileSensorAccuracy::Unreliable;
		return;
	}
	if (Sample.LocationAgeSeconds > 30.0)
	{
		Sample.Header.Accuracy = CapQuality(
			Sample.Header.Accuracy,
			EOpenMobileSensorAccuracy::Medium
		);
	}
}

bool FOpenMobileSensorHeadingQuality::MeetsMinimum(
	EOpenMobileSensorAccuracy Accuracy,
	EOpenMobileSensorAccuracy MinimumAccuracy
)
{
	if (MinimumAccuracy == EOpenMobileSensorAccuracy::Unknown)
	{
		return true;
	}
	return Accuracy != EOpenMobileSensorAccuracy::Unknown
		&& static_cast<uint8>(Accuracy) >=
			static_cast<uint8>(MinimumAccuracy);
}

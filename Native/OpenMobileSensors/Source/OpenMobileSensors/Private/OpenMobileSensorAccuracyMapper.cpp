#include "OpenMobileSensorAccuracyMapper.h"

namespace OpenMobileSensorAccuracyMapperPrivate
{
	FOpenMobileSensorAccuracySnapshot MakeSnapshot(
		const FOpenMobileSensorIdentifier& Sensor,
		double TimestampSeconds
	)
	{
		FOpenMobileSensorAccuracySnapshot Snapshot;
		Snapshot.Sensor = Sensor;
		Snapshot.TimestampSeconds = TimestampSeconds;
		return Snapshot;
	}
}

FOpenMobileSensorAccuracySnapshot
FOpenMobileSensorAccuracyMapper::FromAndroidAccuracyCallback(
	const FOpenMobileSensorIdentifier& Sensor,
	int32 NativeAccuracy,
	double TimestampSeconds
)
{
	FOpenMobileSensorAccuracySnapshot Snapshot =
		OpenMobileSensorAccuracyMapperPrivate::MakeSnapshot(
			Sensor,
			TimestampSeconds
		);
	switch (NativeAccuracy)
	{
	case -1:
	case 0:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Unreliable;
		break;
	case 1:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Low;
		break;
	case 2:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Medium;
		break;
	case 3:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::High;
		break;
	default:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Unknown;
		break;
	}
	return Snapshot;
}

FOpenMobileSensorAccuracySnapshot
FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
	const FOpenMobileSensorIdentifier& Sensor,
	int32 NativeAccuracy,
	double TimestampSeconds
)
{
	FOpenMobileSensorAccuracySnapshot Snapshot =
		OpenMobileSensorAccuracyMapperPrivate::MakeSnapshot(
			Sensor,
			TimestampSeconds
		);
	switch (NativeAccuracy)
	{
	case -1:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Unreliable;
		Snapshot.bCalibrationRequired = true;
		break;
	case 0:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Low;
		break;
	case 1:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Medium;
		break;
	case 2:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::High;
		break;
	default:
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Unknown;
		break;
	}
	return Snapshot;
}

FOpenMobileSensorAccuracySnapshot
FOpenMobileSensorAccuracyMapper::FromIOSHeadingAccuracy(
	const FOpenMobileSensorIdentifier& Sensor,
	double AccuracyDegrees,
	bool bCalibrationRequired,
	double TimestampSeconds
)
{
	FOpenMobileSensorAccuracySnapshot Snapshot =
		OpenMobileSensorAccuracyMapperPrivate::MakeSnapshot(
			Sensor,
			TimestampSeconds
		);
	Snapshot.bCalibrationRequired = bCalibrationRequired;
	if (FMath::IsFinite(AccuracyDegrees) && AccuracyDegrees >= 0.0)
	{
		Snapshot.bHasEstimatedError = true;
		Snapshot.EstimatedError = AccuracyDegrees;
	}
	else
	{
		Snapshot.Accuracy = EOpenMobileSensorAccuracy::Unreliable;
	}
	return Snapshot;
}

#include "OpenMobileSensorOrientationClassifier.h"

namespace OpenMobileSensorOrientationClassifierPrivate
{
	struct FCandidate
	{
		EOpenMobilePhysicalOrientation Orientation =
			EOpenMobilePhysicalOrientation::Unknown;
		double Confidence = 0.0;
	};

	FCandidate Classify(
		const FVector& Gravity,
		const FOpenMobileSensorOrientationClassifierConfig& Config
	)
	{
		FCandidate Result;
		if (Gravity.ContainsNaN()
			|| Gravity.SizeSquared() <= UE_DOUBLE_SMALL_NUMBER)
		{
			return Result;
		}
		const FVector Direction = Gravity.GetSafeNormal();
		const double AbsoluteX = FMath::Abs(Direction.X);
		const double AbsoluteY = FMath::Abs(Direction.Y);
		const double AbsoluteZ = FMath::Abs(Direction.Z);
		const double FaceThreshold = FMath::Cos(
			FMath::DegreesToRadians(Config.FaceAngleDegrees)
		);
		const double EdgeThreshold = FMath::Sin(
			FMath::DegreesToRadians(Config.FaceAngleDegrees)
		);
		if (AbsoluteZ >= FaceThreshold)
		{
			Result.Orientation = Direction.Z < 0.0
				? EOpenMobilePhysicalOrientation::FaceUp
				: EOpenMobilePhysicalOrientation::FaceDown;
			Result.Confidence = AbsoluteZ;
			return Result;
		}
		const double Dominant = FMath::Max(AbsoluteX, AbsoluteY);
		if (AbsoluteZ > EdgeThreshold
			|| Dominant <= UE_DOUBLE_SMALL_NUMBER)
		{
			Result.Confidence = FMath::Clamp(1.0 - AbsoluteZ, 0.0, 1.0);
			return Result;
		}
		const double Secondary = FMath::Min(AbsoluteX, AbsoluteY);
		const double AxisOffsetDegrees = FMath::RadiansToDegrees(
			FMath::Atan2(Secondary, Dominant)
		);
		if (AxisOffsetDegrees > 45.0 - Config.HysteresisDegrees)
		{
			Result.Confidence = FMath::Clamp(1.0 - Dominant, 0.0, 1.0);
			return Result;
		}
		if (AbsoluteX > AbsoluteY)
		{
			Result.Orientation = Direction.X < 0.0
				? EOpenMobilePhysicalOrientation::Portrait
				: EOpenMobilePhysicalOrientation::PortraitUpsideDown;
		}
		else
		{
			Result.Orientation = Direction.Y < 0.0
				? EOpenMobilePhysicalOrientation::LandscapeLeft
				: EOpenMobilePhysicalOrientation::LandscapeRight;
		}
		Result.Confidence = Dominant;
		return Result;
	}

	double ConfidenceFor(
		EOpenMobilePhysicalOrientation Orientation,
		const FVector& Gravity,
		double UnknownConfidence
	)
	{
		if (Orientation == EOpenMobilePhysicalOrientation::Unknown
			|| Gravity.ContainsNaN()
			|| Gravity.SizeSquared() <= UE_DOUBLE_SMALL_NUMBER)
		{
			return Orientation == EOpenMobilePhysicalOrientation::Unknown
				? UnknownConfidence
				: 0.0;
		}
		const FVector Direction = Gravity.GetSafeNormal();
		FVector Expected = FVector::ZeroVector;
		switch (Orientation)
		{
		case EOpenMobilePhysicalOrientation::Portrait:
			Expected = -FVector::ForwardVector;
			break;
		case EOpenMobilePhysicalOrientation::PortraitUpsideDown:
			Expected = FVector::ForwardVector;
			break;
		case EOpenMobilePhysicalOrientation::LandscapeLeft:
			Expected = -FVector::RightVector;
			break;
		case EOpenMobilePhysicalOrientation::LandscapeRight:
			Expected = FVector::RightVector;
			break;
		case EOpenMobilePhysicalOrientation::FaceUp:
			Expected = -FVector::UpVector;
			break;
		case EOpenMobilePhysicalOrientation::FaceDown:
			Expected = FVector::UpVector;
			break;
		case EOpenMobilePhysicalOrientation::Unknown:
		default:
			return UnknownConfidence;
		}
		return FMath::Clamp(Direction | Expected, 0.0, 1.0);
	}
}

bool FOpenMobileSensorOrientationClassifier::Process(
	const FVector& Gravity,
	double TimestampSeconds,
	const FOpenMobileSensorOrientationClassifierConfig& Config,
	FOpenMobileOrientationSensorSample& OutOrientation
)
{
	using namespace OpenMobileSensorOrientationClassifierPrivate;
	OutOrientation = {};
	if (!FMath::IsFinite(TimestampSeconds)
		|| TimestampSeconds < 0.0
		|| !FMath::IsFinite(Config.FaceAngleDegrees)
		|| Config.FaceAngleDegrees <= 0.0
		|| Config.FaceAngleDegrees >= 45.0
		|| !FMath::IsFinite(Config.HysteresisDegrees)
		|| Config.HysteresisDegrees < 0.0
		|| Config.HysteresisDegrees >= 45.0
		|| !FMath::IsFinite(Config.TransitionDebounceSeconds)
		|| Config.TransitionDebounceSeconds < 0.0)
	{
		return false;
	}
	const bool bTimestampReset = bHasState
		&& TimestampSeconds <= LastTimestampSeconds;
	if (bTimestampReset)
	{
		Reset();
	}
	const FCandidate Candidate = Classify(Gravity, Config);
	if (!bHasState)
	{
		bHasState = true;
		Current = Config.TransitionDebounceSeconds <= 0.0
			? Candidate.Orientation
			: EOpenMobilePhysicalOrientation::Unknown;
		if (Current != Candidate.Orientation)
		{
			Pending = Candidate.Orientation;
			PendingSinceSeconds = TimestampSeconds;
			bHasPending = true;
		}
		OutOrientation.Header.bStatefulProcessingReset = true;
	}
	else if (Candidate.Orientation == Current)
	{
		bHasPending = false;
	}
	else if (Config.TransitionDebounceSeconds <= 0.0)
	{
		Current = Candidate.Orientation;
		bHasPending = false;
	}
	else if (!bHasPending || Pending != Candidate.Orientation)
	{
		Pending = Candidate.Orientation;
		PendingSinceSeconds = TimestampSeconds;
		bHasPending = true;
	}
	else if (TimestampSeconds - PendingSinceSeconds >=
		Config.TransitionDebounceSeconds)
	{
		Current = Candidate.Orientation;
		bHasPending = false;
	}
	LastTimestampSeconds = TimestampSeconds;
	OutOrientation.Orientation = Current;
	OutOrientation.Confidence = ConfidenceFor(
		Current,
		Gravity,
		Candidate.Orientation == EOpenMobilePhysicalOrientation::Unknown
			? Candidate.Confidence
			: 0.0
	);
	return true;
}

void FOpenMobileSensorOrientationClassifier::Reset()
{
	Current = EOpenMobilePhysicalOrientation::Unknown;
	Pending = EOpenMobilePhysicalOrientation::Unknown;
	PendingSinceSeconds = 0.0;
	LastTimestampSeconds = 0.0;
	bHasState = false;
	bHasPending = false;
}

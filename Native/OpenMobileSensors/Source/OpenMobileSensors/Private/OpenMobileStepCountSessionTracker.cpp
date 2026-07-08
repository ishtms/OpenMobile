#include "OpenMobileStepCountSessionTracker.h"

namespace OpenMobileStepCountSessionTrackerPrivate
{
	bool IsValidNativeSample(const FOpenMobileStepsSensorSample& Sample)
	{
		if (!Sample.Header.bValid
			|| Sample.Header.Sensor.Type != EOpenMobileSensorType::StepCounter
			|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
			|| Sample.Header.TimestampSeconds < 0.0
			|| Sample.Count < 0
			|| !Sample.OriginIdentifier.IsValid()
			|| (Sample.Origin != EOpenMobileStepCountOrigin::DeviceBoot
				&& Sample.Origin !=
					EOpenMobileStepCountOrigin::QueryInterval))
		{
			return false;
		}
		return Sample.Origin != EOpenMobileStepCountOrigin::QueryInterval
			|| (Sample.bHasQueryInterval
				&& FMath::IsFinite(Sample.QueryStartUnixTimeSeconds)
				&& FMath::IsFinite(Sample.QueryEndUnixTimeSeconds)
				&& Sample.QueryEndUnixTimeSeconds >=
					Sample.QueryStartUnixTimeSeconds);
	}

	int64 SaturatingAdd(int64 Left, int64 Right, bool& bOutSaturated)
	{
		if (Left >= TNumericLimits<int64>::Max() - Right)
		{
			bOutSaturated = Left > TNumericLimits<int64>::Max() - Right;
			return TNumericLimits<int64>::Max();
		}
		return Left + Right;
	}
}

FOpenMobileStepCountSessionTracker::FOpenMobileStepCountSessionTracker(
	const FGuid& InSessionIdentifier
)
{
	Initialize(InSessionIdentifier);
}

void FOpenMobileStepCountSessionTracker::Initialize(
	const FGuid& InSessionIdentifier
)
{
	SessionIdentifier = InSessionIdentifier;
	NativeOriginIdentifier.Invalidate();
	NativeOrigin = EOpenMobileStepCountOrigin::Unknown;
	NativeBaseline = 0;
	LastNativeCount = 0;
	CommittedCount = 0;
	LastSessionCount = 0;
	NativeQueryStartUnixTimeSeconds = 0.0;
	bHasBaseline = false;
	bExplicitResetPending = false;
	bCountSaturated = false;
}

bool FOpenMobileStepCountSessionTracker::Process(
	const FOpenMobileStepsSensorSample& NativeSample,
	FOpenMobileStepsSensorSample& OutSessionSample
)
{
	using namespace OpenMobileStepCountSessionTrackerPrivate;
	if (!SessionIdentifier.IsValid() || !IsValidNativeSample(NativeSample))
	{
		OutSessionSample = {};
		return false;
	}

	const bool bNativeOriginChanged = bHasBaseline
		&& (NativeSample.Origin != NativeOrigin
			|| NativeSample.OriginIdentifier != NativeOriginIdentifier
			|| (NativeSample.Origin ==
					EOpenMobileStepCountOrigin::QueryInterval
				&& NativeSample.QueryStartUnixTimeSeconds !=
					NativeQueryStartUnixTimeSeconds));
	const bool bNativeRollback = bHasBaseline
		&& !bNativeOriginChanged
		&& NativeSample.Count < LastNativeCount;
	const bool bNativeDiscontinuity = bHasBaseline
		&& (bNativeOriginChanged
			|| bNativeRollback
			|| NativeSample.Discontinuity !=
				EOpenMobileStepCountDiscontinuity::None
			|| NativeSample.Header.bStatefulProcessingReset);

	EOpenMobileStepCountDiscontinuity SessionDiscontinuity =
		EOpenMobileStepCountDiscontinuity::None;
	int64 SessionCount = 0;
	if (!bHasBaseline)
	{
		NativeBaseline = NativeSample.Count;
		CommittedCount = 0;
		LastSessionCount = 0;
		bCountSaturated = false;
		SessionDiscontinuity = bExplicitResetPending
			? EOpenMobileStepCountDiscontinuity::SessionReset
			: EOpenMobileStepCountDiscontinuity::StreamStarted;
		bExplicitResetPending = false;
		bHasBaseline = true;
	}
	else if (bNativeDiscontinuity)
	{
		CommittedCount = LastSessionCount;
		NativeBaseline = NativeSample.Count;
		SessionCount = CommittedCount;
		if (bNativeRollback
			|| NativeSample.Discontinuity ==
				EOpenMobileStepCountDiscontinuity::NativeCounterReset)
		{
			SessionDiscontinuity =
				EOpenMobileStepCountDiscontinuity::NativeCounterReset;
		}
		else if (bNativeOriginChanged
			|| NativeSample.Discontinuity ==
				EOpenMobileStepCountDiscontinuity::OriginChanged)
		{
			SessionDiscontinuity =
				EOpenMobileStepCountDiscontinuity::OriginChanged;
		}
		else
		{
			SessionDiscontinuity =
				EOpenMobileStepCountDiscontinuity::StreamStarted;
		}
	}
	else
	{
		const int64 NativeDelta = NativeSample.Count - NativeBaseline;
		SessionCount = SaturatingAdd(
			CommittedCount,
			NativeDelta,
			bCountSaturated);
	}

	NativeOrigin = NativeSample.Origin;
	NativeOriginIdentifier = NativeSample.OriginIdentifier;
	NativeQueryStartUnixTimeSeconds = NativeSample.Origin ==
		EOpenMobileStepCountOrigin::QueryInterval
		? NativeSample.QueryStartUnixTimeSeconds
		: 0.0;
	LastNativeCount = NativeSample.Count;
	LastSessionCount = SessionCount;

	OutSessionSample = NativeSample;
	OutSessionSample.Count = SessionCount;
	OutSessionSample.Origin = EOpenMobileStepCountOrigin::Session;
	OutSessionSample.OriginIdentifier = SessionIdentifier;
	OutSessionSample.Discontinuity = SessionDiscontinuity;
	OutSessionSample.bCountSaturated = bCountSaturated;
	OutSessionSample.bHasQueryInterval = false;
	OutSessionSample.QueryStartUnixTimeSeconds = 0.0;
	OutSessionSample.QueryEndUnixTimeSeconds = 0.0;
	OutSessionSample.Header.bStatefulProcessingReset |=
		SessionDiscontinuity != EOpenMobileStepCountDiscontinuity::None;
	return true;
}

void FOpenMobileStepCountSessionTracker::ResetBaseline()
{
	NativeOriginIdentifier.Invalidate();
	NativeOrigin = EOpenMobileStepCountOrigin::Unknown;
	NativeBaseline = 0;
	LastNativeCount = 0;
	CommittedCount = 0;
	LastSessionCount = 0;
	NativeQueryStartUnixTimeSeconds = 0.0;
	bHasBaseline = false;
	bExplicitResetPending = true;
	bCountSaturated = false;
}

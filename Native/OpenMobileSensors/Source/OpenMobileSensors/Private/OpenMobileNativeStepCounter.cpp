#include "OpenMobileNativeStepCounter.h"

namespace OpenMobileNativeStepCounterPrivate
{
	constexpr double Int64UpperBound = 9223372036854775808.0;
}

bool FOpenMobileNativeStepCounterTracker::TryConvertNativeTotal(
	double NativeTotal,
	int64& OutTotal
)
{
	using namespace OpenMobileNativeStepCounterPrivate;
	if (!FMath::IsFinite(NativeTotal)
		|| NativeTotal < 0.0
		|| NativeTotal >= Int64UpperBound
		|| NativeTotal != FMath::FloorToDouble(NativeTotal))
	{
		return false;
	}
	OutTotal = static_cast<int64>(NativeTotal);
	return true;
}

bool FOpenMobileNativeStepCounterTracker::Apply(
	FOpenMobileStepsSensorSample& Sample
)
{
	if (Sample.Count < 0
		|| Sample.Origin == EOpenMobileStepCountOrigin::Unknown
		|| (Sample.Origin == EOpenMobileStepCountOrigin::QueryInterval
			&& (!Sample.bHasQueryInterval
				|| !FMath::IsFinite(Sample.QueryStartUnixTimeSeconds)
				|| !FMath::IsFinite(Sample.QueryEndUnixTimeSeconds)
				|| Sample.QueryEndUnixTimeSeconds <
					Sample.QueryStartUnixTimeSeconds)))
	{
		return false;
	}

	EOpenMobileStepCountDiscontinuity Discontinuity =
		EOpenMobileStepCountDiscontinuity::None;
	if (!bHasSample)
	{
		Discontinuity = EOpenMobileStepCountDiscontinuity::StreamStarted;
	}
	else
	{
		const bool bDeclaredOriginChanged =
			Sample.OriginIdentifier.IsValid()
			&& Sample.OriginIdentifier != OriginIdentifier;
		const bool bQueryStartChanged =
			Sample.Origin == EOpenMobileStepCountOrigin::QueryInterval
			&& (!bHasQueryInterval
				|| Sample.QueryStartUnixTimeSeconds !=
					QueryStartUnixTimeSeconds);
		if (Sample.Origin != Origin
			|| bDeclaredOriginChanged
			|| bQueryStartChanged)
		{
			Discontinuity = EOpenMobileStepCountDiscontinuity::OriginChanged;
		}
		else if (Sample.Count < LastCount)
		{
			Discontinuity =
				EOpenMobileStepCountDiscontinuity::NativeCounterReset;
		}
	}

	if (Discontinuity != EOpenMobileStepCountDiscontinuity::None)
	{
		if (!Sample.OriginIdentifier.IsValid()
			|| Sample.OriginIdentifier == OriginIdentifier)
		{
			Sample.OriginIdentifier = FGuid::NewGuid();
		}
		OriginIdentifier = Sample.OriginIdentifier;
		Sample.Header.bStatefulProcessingReset = true;
	}
	else
	{
		Sample.OriginIdentifier = OriginIdentifier;
	}

	Sample.Discontinuity = Discontinuity;
	Origin = Sample.Origin;
	LastCount = Sample.Count;
	bHasQueryInterval = Sample.Origin ==
		EOpenMobileStepCountOrigin::QueryInterval;
	QueryStartUnixTimeSeconds = bHasQueryInterval
		? Sample.QueryStartUnixTimeSeconds
		: 0.0;
	bHasSample = true;
	return true;
}

void FOpenMobileNativeStepCounterTracker::Reset()
{
	OriginIdentifier.Invalidate();
	Origin = EOpenMobileStepCountOrigin::Unknown;
	LastCount = 0;
	QueryStartUnixTimeSeconds = 0.0;
	bHasQueryInterval = false;
	bHasSample = false;
}

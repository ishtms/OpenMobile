#include "OpenMobileDeviceThermalHeadroom.h"

bool FOpenMobileDeviceThermalHeadroomTracker::ShouldSample(
	double MonotonicSeconds
) const
{
	return FMath::IsFinite(MonotonicSeconds)
		&& (!LastAttemptSeconds.IsSet()
			|| MonotonicSeconds - LastAttemptSeconds.GetValue()
				>= MinimumSampleIntervalSeconds);
}

void FOpenMobileDeviceThermalHeadroomTracker::ApplyNativeSample(
	FOpenMobilePowerSnapshot& Snapshot,
	float Headroom,
	int32 ForecastSeconds,
	double MonotonicSeconds,
	const FDateTime& SampleTimeUtc,
	bool bAvailable
)
{
	if (FMath::IsFinite(MonotonicSeconds))
	{
		LastAttemptSeconds = MonotonicSeconds;
	}

	const bool bValid = bAvailable
		&& FMath::IsFinite(Headroom)
		&& Headroom >= 0.0f
		&& ForecastSeconds >= 0
		&& ForecastSeconds <= MaximumForecastSeconds
		&& FMath::IsFinite(MonotonicSeconds)
		&& SampleTimeUtc.GetTicks() > 0;
	if (!bValid)
	{
		LatestHeadroom = {};
		LatestForecastSeconds = {};
		LatestSampleTimeUtc = {};
		LatestTrend = EOpenMobileThermalTrend::Unknown;
		LastValidSampleSeconds.Reset();
		LastValidHeadroom.Reset();
		ApplyLatest(Snapshot);
		return;
	}

	LatestTrend = EOpenMobileThermalTrend::Unknown;
	if (LastValidSampleSeconds.IsSet() && LastValidHeadroom.IsSet())
	{
		const double GapSeconds =
			MonotonicSeconds - LastValidSampleSeconds.GetValue();
		if (GapSeconds > 0.0 && GapSeconds <= MaximumTrendGapSeconds)
		{
			const float Delta = Headroom - LastValidHeadroom.GetValue();
			LatestTrend = Delta >= StableDelta
				? EOpenMobileThermalTrend::Heating
				: Delta <= -StableDelta
					? EOpenMobileThermalTrend::Cooling
					: EOpenMobileThermalTrend::Stable;
		}
	}

	LastValidSampleSeconds = MonotonicSeconds;
	LastValidHeadroom = Headroom;
	LatestHeadroom = FOpenMobileDeviceOptionalFloat::MakeAvailable(Headroom);
	LatestForecastSeconds = FOpenMobileDeviceOptionalFloat::MakeAvailable(
		static_cast<float>(ForecastSeconds)
	);
	LatestSampleTimeUtc = SampleTimeUtc;
	ApplyLatest(Snapshot);
}

void FOpenMobileDeviceThermalHeadroomTracker::ApplyLatest(
	FOpenMobilePowerSnapshot& Snapshot
) const
{
	Snapshot.ThermalHeadroom = LatestHeadroom;
	Snapshot.ThermalForecastSeconds = LatestForecastSeconds;
	Snapshot.ThermalHeadroomSampleTimeUtc = LatestSampleTimeUtc;
	Snapshot.ThermalTrend = LatestTrend;
}

void FOpenMobileDeviceThermalHeadroomTracker::ResetTrend()
{
	LastValidSampleSeconds.Reset();
	LastValidHeadroom.Reset();
	LatestTrend = EOpenMobileThermalTrend::Unknown;
}

void FOpenMobileDeviceThermalHeadroomTracker::Reset()
{
	LastAttemptSeconds.Reset();
	LastValidSampleSeconds.Reset();
	LastValidHeadroom.Reset();
	LatestHeadroom = {};
	LatestForecastSeconds = {};
	LatestSampleTimeUtc = {};
	LatestTrend = EOpenMobileThermalTrend::Unknown;
}

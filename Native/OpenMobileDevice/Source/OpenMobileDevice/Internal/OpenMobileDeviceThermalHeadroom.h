#pragma once

#include "OpenMobileDeviceResourceTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceThermalHeadroomTracker final
{
public:
	static constexpr int32 DefaultForecastSeconds = 10;
	static constexpr int32 MaximumForecastSeconds = 60;
	static constexpr double MinimumSampleIntervalSeconds = 10.0;
	static constexpr double MaximumTrendGapSeconds = 30.0;
	static constexpr float StableDelta = 0.02f;

	bool ShouldSample(double MonotonicSeconds) const;
	void ApplyNativeSample(
		FOpenMobilePowerSnapshot& Snapshot,
		float Headroom,
		int32 ForecastSeconds,
		double MonotonicSeconds,
		const FDateTime& SampleTimeUtc,
		bool bAvailable
	);
	void ApplyLatest(FOpenMobilePowerSnapshot& Snapshot) const;
	void ResetTrend();
	void Reset();

private:
	TOptional<double> LastAttemptSeconds;
	TOptional<double> LastValidSampleSeconds;
	TOptional<float> LastValidHeadroom;
	FOpenMobileDeviceOptionalFloat LatestHeadroom;
	FOpenMobileDeviceOptionalFloat LatestForecastSeconds;
	FDateTime LatestSampleTimeUtc;
	EOpenMobileThermalTrend LatestTrend = EOpenMobileThermalTrend::Unknown;
};

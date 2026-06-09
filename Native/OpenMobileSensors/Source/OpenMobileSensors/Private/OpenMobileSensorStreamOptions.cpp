#include "OpenMobileSensorStreamOptions.h"

bool UOpenMobileSensorRateLibrary::HertzToIntervalSeconds(
	double FrequencyHz,
	double& OutIntervalSeconds
)
{
	OutIntervalSeconds = 0.0;
	if (!FMath::IsFinite(FrequencyHz) || FrequencyHz <= 0.0)
	{
		return false;
	}
	OutIntervalSeconds = 1.0 / FrequencyHz;
	if (!FMath::IsFinite(OutIntervalSeconds))
	{
		OutIntervalSeconds = 0.0;
		return false;
	}
	return true;
}

bool UOpenMobileSensorRateLibrary::IntervalSecondsToHertz(
	double IntervalSeconds,
	double& OutFrequencyHz
)
{
	OutFrequencyHz = 0.0;
	if (!FMath::IsFinite(IntervalSeconds) || IntervalSeconds <= 0.0)
	{
		return false;
	}
	OutFrequencyHz = 1.0 / IntervalSeconds;
	if (!FMath::IsFinite(OutFrequencyHz))
	{
		OutFrequencyHz = 0.0;
		return false;
	}
	return true;
}

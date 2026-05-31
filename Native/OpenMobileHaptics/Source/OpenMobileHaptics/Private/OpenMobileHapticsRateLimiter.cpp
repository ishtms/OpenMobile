#include "OpenMobileHapticsRateLimiter.h"

bool FOpenMobileHapticsRateLimiter::ShouldSuppress(
	FName Channel,
	bool bSelection,
	double TimeSeconds,
	double MinimumIntervalSeconds,
	double SelectionDebounceSeconds,
	int32 MaximumSubmissionsPerSecond
)
{
	RecentSubmissionTimes.RemoveAll(
		[TimeSeconds](double PreviousTime)
		{
			return PreviousTime <= TimeSeconds - 1.0;
		}
	);
	if (RecentSubmissionTimes.Num() >= MaximumSubmissionsPerSecond)
	{
		return true;
	}
	if (const double* PreviousTime = LastSubmissionByChannel.Find(Channel))
	{
		if (TimeSeconds - *PreviousTime + UE_DOUBLE_SMALL_NUMBER
			< MinimumIntervalSeconds)
		{
			return true;
		}
	}
	if (bSelection)
	{
		if (const double* PreviousTime = LastSelectionByChannel.Find(Channel))
		{
			if (TimeSeconds - *PreviousTime + UE_DOUBLE_SMALL_NUMBER
				< SelectionDebounceSeconds)
			{
				return true;
			}
		}
		LastSelectionByChannel.Add(Channel, TimeSeconds);
	}
	LastSubmissionByChannel.Add(Channel, TimeSeconds);
	RecentSubmissionTimes.Add(TimeSeconds);
	return false;
}

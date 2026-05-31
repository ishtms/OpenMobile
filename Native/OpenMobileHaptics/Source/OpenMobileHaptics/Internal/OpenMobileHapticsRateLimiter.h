#pragma once

#include "CoreMinimal.h"

class FOpenMobileHapticsRateLimiter
{
public:
	bool ShouldSuppress(
		FName Channel,
		bool bSelection,
		double TimeSeconds,
		double MinimumIntervalSeconds,
		double SelectionDebounceSeconds,
		int32 MaximumSubmissionsPerSecond
	);

private:
	TMap<FName, double> LastSubmissionByChannel;
	TMap<FName, double> LastSelectionByChannel;
	TArray<double> RecentSubmissionTimes;
};

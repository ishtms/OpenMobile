#pragma once

#include "OpenMobileSensorSamples.h"

struct FOpenMobileActivityFilterConfig
{
	EOpenMobileActivityConfidence MinimumConfidence =
		EOpenMobileActivityConfidence::Unknown;
	double MinimumStableDurationSeconds = 0.0;
};

class FOpenMobileActivitySampleFilter
{
public:
	void Configure(const FOpenMobileActivityFilterConfig& InConfig);
	bool Process(FOpenMobileActivitySensorSample& Sample);
	void Reset();

private:
	FOpenMobileActivityFilterConfig Config;
	EOpenMobileMotionActivity CandidateActivity =
		EOpenMobileMotionActivity::Unknown;
	EOpenMobileMotionActivity AcceptedActivity =
		EOpenMobileMotionActivity::Unknown;
	EOpenMobileActivityConfidence AcceptedConfidence =
		EOpenMobileActivityConfidence::Unknown;
	TArray<EOpenMobileMotionActivity> AcceptedConcurrentActivities;
	double CandidateStartTimestampSeconds = 0.0;
	double LastObservedTimestampSeconds = 0.0;
	bool bHasCandidate = false;
	bool bHasAccepted = false;
	bool bCandidateChangedAfterLastAccepted = false;
	bool bConfidenceSuppressed = false;
};

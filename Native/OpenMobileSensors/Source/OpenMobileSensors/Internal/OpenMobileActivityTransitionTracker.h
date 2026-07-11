#pragma once

#include "OpenMobileSensorSamples.h"

class FOpenMobileActivityTransitionTracker
{
public:
	void Configure(double InDebounceSeconds);
	bool Process(
		FOpenMobileActivitySensorSample Sample,
		FOpenMobileActivitySensorBatch& OutBatch
	);
	void Reset();

private:
	FOpenMobileActivitySensorSample CommittedSample;
	double DebounceSeconds = 0.25;
	double LastObservedTimestampSeconds = 0.0;
	double LastTransitionTimestampSeconds = 0.0;
	bool bHasCommittedSample = false;
	bool bHasTransitionTimestamp = false;
};

class FOpenMobileActivityTransitionEventFilter
{
public:
	void Configure(EOpenMobileActivityConfidence InMinimumConfidence);
	bool Process(FOpenMobileActivitySensorSample& Sample);
	void Reset();

private:
	TSet<EOpenMobileMotionActivity> ObservedActivities;
	TSet<EOpenMobileMotionActivity> ActiveActivities;
	EOpenMobileActivityConfidence MinimumConfidence =
		EOpenMobileActivityConfidence::Unknown;
	EOpenMobileMotionActivity LastActivity =
		EOpenMobileMotionActivity::Unknown;
	EOpenMobileActivityTransition LastTransition =
		EOpenMobileActivityTransition::None;
	double LastTimestampSeconds = 0.0;
	bool bHasLastEvent = false;
};

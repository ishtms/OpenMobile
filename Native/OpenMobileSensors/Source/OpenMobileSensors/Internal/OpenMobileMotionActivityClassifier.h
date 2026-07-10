#pragma once

#include "OpenMobileSensorSamples.h"

struct FOpenMobileNativeMotionActivityState
{
	bool bUnknown = false;
	bool bStationary = false;
	bool bWalking = false;
	bool bRunning = false;
	bool bCycling = false;
	bool bAutomotive = false;
	int32 Confidence = -1;
};

class FOpenMobileMotionActivityClassifier
{
public:
	static void Classify(
		const FOpenMobileNativeMotionActivityState& Native,
		FOpenMobileActivitySensorSample& OutSample
	);
};

class FOpenMobileMotionActivityTracker
{
public:
	bool Accept(const FOpenMobileActivitySensorSample& Sample);
	void Reset();

private:
	EOpenMobileMotionActivity Activity = EOpenMobileMotionActivity::Unknown;
	EOpenMobileActivityConfidence Confidence =
		EOpenMobileActivityConfidence::Unknown;
	TArray<EOpenMobileMotionActivity> ConcurrentActivities;
	bool bHasState = false;
};

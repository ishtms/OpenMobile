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
	static void NormalizeSample(FOpenMobileActivitySensorSample& Sample);
};

#pragma once

#include "CoreMinimal.h"

struct FOpenMobileHapticsPerformanceSnapshot
{
	uint64 DroppedRequestCount = 0;
	int32 PeakQueuedPlaybackCount = 0;
	uint64 PreparationCount = 0;
	double LastPreparationLatencyMilliseconds = 0.0;
	double MaximumPreparationLatencyMilliseconds = 0.0;
	uint64 NativeSubmissionCount = 0;
	double LastNativeSubmissionLatencyMilliseconds = 0.0;
	double MaximumNativeSubmissionLatencyMilliseconds = 0.0;
};

class FOpenMobileHapticsPerformanceTracker final
{
public:
	void RecordDroppedRequest();
	void RecordQueueDepth(int32 QueueDepth);
	void RecordPreparationLatencySeconds(double LatencySeconds);
	void RecordNativeSubmissionLatencySeconds(double LatencySeconds);
	FOpenMobileHapticsPerformanceSnapshot GetSnapshot() const;

private:
	FOpenMobileHapticsPerformanceSnapshot Snapshot;
};

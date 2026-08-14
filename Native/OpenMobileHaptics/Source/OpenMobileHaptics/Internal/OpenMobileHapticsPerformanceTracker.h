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
	/** Counts work rejected before submission, which helps separate policy pressure from native failures. */
	void RecordDroppedRequest();
	/** Tracks only the highest observed queue depth, ongoing depth already lives in subsystem state. */
	void RecordQueueDepth(int32 QueueDepth);
	/** Stores latest and worst preparation latency after rejecting invalid timing samples. */
	void RecordPreparationLatencySeconds(double LatencySeconds);
	/** Stores latest and worst native submission latency without retaining every request. */
	void RecordNativeSubmissionLatencySeconds(double LatencySeconds);
	/** Returns a value copy so diagnostics can't mutate live counters. */
	FOpenMobileHapticsPerformanceSnapshot GetSnapshot() const;

private:
	FOpenMobileHapticsPerformanceSnapshot Snapshot;
};

#include "OpenMobileHapticsPerformanceTracker.h"

namespace OpenMobileHapticsPerformanceTrackerPrivate
{
	bool ToMilliseconds(double Seconds, double& OutMilliseconds)
	{
		if (!FMath::IsFinite(Seconds) || Seconds < 0.0)
		{
			return false;
		}
		OutMilliseconds = Seconds <= MAX_dbl / 1000.0
			? Seconds * 1000.0
			: MAX_dbl;
		return true;
	}

	void Increment(uint64& Value)
	{
		if (Value < MAX_uint64)
		{
			++Value;
		}
	}
}

void FOpenMobileHapticsPerformanceTracker::RecordDroppedRequest()
{
	OpenMobileHapticsPerformanceTrackerPrivate::Increment(
		Snapshot.DroppedRequestCount
	);
}

void FOpenMobileHapticsPerformanceTracker::RecordQueueDepth(int32 QueueDepth)
{
	if (QueueDepth >= 0)
	{
		Snapshot.PeakQueuedPlaybackCount = FMath::Max(
			Snapshot.PeakQueuedPlaybackCount,
			QueueDepth
		);
	}
}

void FOpenMobileHapticsPerformanceTracker::RecordPreparationLatencySeconds(
	double LatencySeconds
)
{
	double Milliseconds = 0.0;
	if (!OpenMobileHapticsPerformanceTrackerPrivate::ToMilliseconds(
		LatencySeconds,
		Milliseconds
	))
	{
		return;
	}
	OpenMobileHapticsPerformanceTrackerPrivate::Increment(
		Snapshot.PreparationCount
	);
	Snapshot.LastPreparationLatencyMilliseconds = Milliseconds;
	Snapshot.MaximumPreparationLatencyMilliseconds = FMath::Max(
		Snapshot.MaximumPreparationLatencyMilliseconds,
		Milliseconds
	);
}

void FOpenMobileHapticsPerformanceTracker::RecordNativeSubmissionLatencySeconds(
	double LatencySeconds
)
{
	double Milliseconds = 0.0;
	if (!OpenMobileHapticsPerformanceTrackerPrivate::ToMilliseconds(
		LatencySeconds,
		Milliseconds
	))
	{
		return;
	}
	OpenMobileHapticsPerformanceTrackerPrivate::Increment(
		Snapshot.NativeSubmissionCount
	);
	Snapshot.LastNativeSubmissionLatencyMilliseconds = Milliseconds;
	Snapshot.MaximumNativeSubmissionLatencyMilliseconds = FMath::Max(
		Snapshot.MaximumNativeSubmissionLatencyMilliseconds,
		Milliseconds
	);
}

FOpenMobileHapticsPerformanceSnapshot
FOpenMobileHapticsPerformanceTracker::GetSnapshot() const
{
	return Snapshot;
}

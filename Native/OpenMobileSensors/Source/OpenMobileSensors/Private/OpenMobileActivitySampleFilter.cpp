#include "OpenMobileActivitySampleFilter.h"

#include "OpenMobileMotionActivityClassifier.h"

void FOpenMobileActivitySampleFilter::Configure(
	const FOpenMobileActivityFilterConfig& InConfig
)
{
	Config = InConfig;
	Reset();
}

bool FOpenMobileActivitySampleFilter::Process(
	FOpenMobileActivitySensorSample& Sample
)
{
	const bool bReset = Sample.Header.bStatefulProcessingReset;
	if (bReset)
	{
		Reset();
	}
	FOpenMobileMotionActivityClassifier::NormalizeSample(Sample);
	if (Sample.Header.Sensor.Type != EOpenMobileSensorType::MotionActivity
		|| !Sample.Header.bValid
		|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
		|| Sample.Header.TimestampSeconds < 0.0
		|| (bHasCandidate
			&& Sample.Header.TimestampSeconds <=
				LastObservedTimestampSeconds))
	{
		return false;
	}
	if (!bHasCandidate || Sample.Activity != CandidateActivity)
	{
		CandidateActivity = Sample.Activity;
		CandidateStartTimestampSeconds = Sample.Header.TimestampSeconds;
		bHasCandidate = true;
		bCandidateChangedAfterLastAccepted = true;
	}
	LastObservedTimestampSeconds = Sample.Header.TimestampSeconds;
	const bool bConfidenceEligible = static_cast<uint8>(Sample.Confidence)
		>= static_cast<uint8>(Config.MinimumConfidence);
	if (!bConfidenceEligible)
	{
		bConfidenceSuppressed = true;
		return false;
	}
	const bool bStable =
		Sample.Header.TimestampSeconds - CandidateStartTimestampSeconds
			>= Config.MinimumStableDurationSeconds;
	if (!bStable)
	{
		return false;
	}
	const bool bChanged = !bHasAccepted
		|| bCandidateChangedAfterLastAccepted
		|| bConfidenceSuppressed
		|| Sample.Activity != AcceptedActivity
		|| Sample.Confidence != AcceptedConfidence
		|| Sample.ConcurrentActivities != AcceptedConcurrentActivities;
	if (!bChanged && !bReset)
	{
		return false;
	}
	AcceptedActivity = Sample.Activity;
	AcceptedConfidence = Sample.Confidence;
	AcceptedConcurrentActivities = Sample.ConcurrentActivities;
	bHasAccepted = true;
	bCandidateChangedAfterLastAccepted = false;
	bConfidenceSuppressed = false;
	return true;
}

void FOpenMobileActivitySampleFilter::Reset()
{
	CandidateActivity = EOpenMobileMotionActivity::Unknown;
	AcceptedActivity = EOpenMobileMotionActivity::Unknown;
	AcceptedConfidence = EOpenMobileActivityConfidence::Unknown;
	AcceptedConcurrentActivities.Reset();
	CandidateStartTimestampSeconds = 0.0;
	LastObservedTimestampSeconds = 0.0;
	bHasCandidate = false;
	bHasAccepted = false;
	bCandidateChangedAfterLastAccepted = false;
	bConfidenceSuppressed = false;
}

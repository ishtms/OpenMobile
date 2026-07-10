#include "OpenMobileMotionActivityClassifier.h"

void FOpenMobileMotionActivityClassifier::Classify(
	const FOpenMobileNativeMotionActivityState& Native,
	FOpenMobileActivitySensorSample& OutSample
)
{
	OutSample.ConcurrentActivities.Reset();
	auto Add = [&OutSample](
		bool bPresent,
		EOpenMobileMotionActivity Activity
	)
	{
		if (bPresent)
		{
			OutSample.ConcurrentActivities.Add(Activity);
		}
	};
	Add(Native.bStationary, EOpenMobileMotionActivity::Stationary);
	Add(Native.bWalking, EOpenMobileMotionActivity::Walking);
	Add(Native.bRunning, EOpenMobileMotionActivity::Running);
	Add(Native.bCycling, EOpenMobileMotionActivity::Cycling);
	Add(Native.bAutomotive, EOpenMobileMotionActivity::Automotive);

	if (Native.bRunning)
	{
		OutSample.Activity = EOpenMobileMotionActivity::Running;
	}
	else if (Native.bCycling)
	{
		OutSample.Activity = EOpenMobileMotionActivity::Cycling;
	}
	else if (Native.bAutomotive)
	{
		OutSample.Activity = EOpenMobileMotionActivity::Automotive;
	}
	else if (Native.bWalking)
	{
		OutSample.Activity = EOpenMobileMotionActivity::Walking;
	}
	else if (Native.bStationary)
	{
		OutSample.Activity = EOpenMobileMotionActivity::Stationary;
	}
	else
	{
		OutSample.Activity = EOpenMobileMotionActivity::Unknown;
	}

	switch (Native.Confidence)
	{
	case 0:
		OutSample.Confidence = EOpenMobileActivityConfidence::Low;
		break;
	case 1:
		OutSample.Confidence = EOpenMobileActivityConfidence::Medium;
		break;
	case 2:
		OutSample.Confidence = EOpenMobileActivityConfidence::High;
		break;
	default:
		OutSample.Confidence = EOpenMobileActivityConfidence::Unknown;
		break;
	}
}

bool FOpenMobileMotionActivityTracker::Accept(
	const FOpenMobileActivitySensorSample& Sample
)
{
	if (Sample.Header.Sensor.Type != EOpenMobileSensorType::MotionActivity
		|| !Sample.Header.bValid
		|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
		|| Sample.Header.TimestampSeconds < 0.0)
	{
		return false;
	}
	const bool bChanged = !bHasState
		|| Sample.Activity != Activity
		|| Sample.Confidence != Confidence
		|| Sample.ConcurrentActivities != ConcurrentActivities;
	if (!bChanged && !Sample.Header.bStatefulProcessingReset)
	{
		return false;
	}
	Activity = Sample.Activity;
	Confidence = Sample.Confidence;
	ConcurrentActivities = Sample.ConcurrentActivities;
	bHasState = true;
	return true;
}

void FOpenMobileMotionActivityTracker::Reset()
{
	Activity = EOpenMobileMotionActivity::Unknown;
	Confidence = EOpenMobileActivityConfidence::Unknown;
	ConcurrentActivities.Reset();
	bHasState = false;
}

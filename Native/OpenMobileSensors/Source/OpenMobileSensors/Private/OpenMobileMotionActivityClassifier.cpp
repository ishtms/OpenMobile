#include "OpenMobileMotionActivityClassifier.h"

namespace OpenMobileMotionActivityClassifierPrivate
{
	bool IsKnownActivity(EOpenMobileMotionActivity Activity)
	{
		switch (Activity)
		{
		case EOpenMobileMotionActivity::Unknown:
		case EOpenMobileMotionActivity::Stationary:
		case EOpenMobileMotionActivity::Walking:
		case EOpenMobileMotionActivity::Running:
		case EOpenMobileMotionActivity::Cycling:
		case EOpenMobileMotionActivity::Automotive:
			return true;
		default:
			return false;
		}
	}

	bool IsKnownConfidence(EOpenMobileActivityConfidence Confidence)
	{
		switch (Confidence)
		{
		case EOpenMobileActivityConfidence::Unknown:
		case EOpenMobileActivityConfidence::Low:
		case EOpenMobileActivityConfidence::Medium:
		case EOpenMobileActivityConfidence::High:
			return true;
		default:
			return false;
		}
	}

	EOpenMobileMotionActivity ChoosePrimary(
		const TArray<EOpenMobileMotionActivity>& Activities
	)
	{
		for (const EOpenMobileMotionActivity Candidate : {
			EOpenMobileMotionActivity::Running,
			EOpenMobileMotionActivity::Cycling,
			EOpenMobileMotionActivity::Automotive,
			EOpenMobileMotionActivity::Walking,
			EOpenMobileMotionActivity::Stationary
		})
		{
			if (Activities.Contains(Candidate))
			{
				return Candidate;
			}
		}
		return EOpenMobileMotionActivity::Unknown;
	}
}

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
	NormalizeSample(OutSample);
}

void FOpenMobileMotionActivityClassifier::NormalizeSample(
	FOpenMobileActivitySensorSample& Sample
)
{
	using namespace OpenMobileMotionActivityClassifierPrivate;
	TArray<EOpenMobileMotionActivity> CanonicalActivities;
	for (const EOpenMobileMotionActivity Activity
		: Sample.ConcurrentActivities)
	{
		if (Activity != EOpenMobileMotionActivity::Unknown
			&& IsKnownActivity(Activity))
		{
			CanonicalActivities.AddUnique(Activity);
		}
	}
	CanonicalActivities.Sort(
		[](EOpenMobileMotionActivity Left, EOpenMobileMotionActivity Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		}
	);
	Sample.ConcurrentActivities = MoveTemp(CanonicalActivities);
	if (!Sample.ConcurrentActivities.IsEmpty())
	{
		Sample.Activity = ChoosePrimary(Sample.ConcurrentActivities);
	}
	else if (!IsKnownActivity(Sample.Activity))
	{
		Sample.Activity = EOpenMobileMotionActivity::Unknown;
	}
	if (!IsKnownConfidence(Sample.Confidence))
	{
		Sample.Confidence = EOpenMobileActivityConfidence::Unknown;
	}
}

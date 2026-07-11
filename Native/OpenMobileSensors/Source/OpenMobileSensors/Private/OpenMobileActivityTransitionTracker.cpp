#include "OpenMobileActivityTransitionTracker.h"

#include "OpenMobileMotionActivityClassifier.h"

namespace OpenMobileActivityTransitionTrackerPrivate
{
	bool IsKnownActivity(EOpenMobileMotionActivity Activity)
	{
		return Activity != EOpenMobileMotionActivity::Unknown;
	}

	FOpenMobileActivitySensorSample MakeTransition(
		const FOpenMobileActivitySensorSample& State,
		const FOpenMobileSensorSampleHeader& Header,
		EOpenMobileActivityTransition Transition
	)
	{
		FOpenMobileActivitySensorSample Output = State;
		Output.Header = Header;
		constexpr int32 OverlayFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Mock
		) | static_cast<int32>(EOpenMobileSensorSourceFlags::Replay);
		Output.Header.SourceFlags =
			(Header.SourceFlags & OverlayFlags)
			| static_cast<int32>(
				EOpenMobileSensorSourceFlags::PluginDerived
			);
		Output.Transition = Transition;
		Output.TransitionOrigin =
			EOpenMobileActivityTransitionOrigin::Derived;
		return Output;
	}
}

void FOpenMobileActivityTransitionTracker::Configure(
	double InDebounceSeconds
)
{
	DebounceSeconds = FMath::Max(0.0, InDebounceSeconds);
	Reset();
}

bool FOpenMobileActivityTransitionTracker::Process(
	FOpenMobileActivitySensorSample Sample,
	FOpenMobileActivitySensorBatch& OutBatch
)
{
	using namespace OpenMobileActivityTransitionTrackerPrivate;
	OutBatch.Samples.Reset();
	FOpenMobileMotionActivityClassifier::NormalizeSample(Sample);
	if (Sample.Header.Sensor.Type != EOpenMobileSensorType::MotionActivity
		|| !Sample.Header.bValid
		|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
		|| Sample.Header.TimestampSeconds < 0.0)
	{
		return false;
	}
	if (Sample.Header.bStatefulProcessingReset)
	{
		Reset();
		CommittedSample = MoveTemp(Sample);
		LastObservedTimestampSeconds =
			CommittedSample.Header.TimestampSeconds;
		bHasCommittedSample = true;
		return false;
	}
	if (bHasCommittedSample
		&& Sample.Header.TimestampSeconds <= LastObservedTimestampSeconds)
	{
		return false;
	}
	LastObservedTimestampSeconds = Sample.Header.TimestampSeconds;
	if (!bHasCommittedSample)
	{
		CommittedSample = MoveTemp(Sample);
		bHasCommittedSample = true;
		return false;
	}
	if (Sample.Activity == CommittedSample.Activity)
	{
		CommittedSample = MoveTemp(Sample);
		return false;
	}
	if (bHasTransitionTimestamp
		&& Sample.Header.TimestampSeconds - LastTransitionTimestampSeconds
			< DebounceSeconds)
	{
		return false;
	}
	if (IsKnownActivity(CommittedSample.Activity))
	{
		OutBatch.Samples.Add(MakeTransition(
			CommittedSample,
			Sample.Header,
			EOpenMobileActivityTransition::Stopped
		));
	}
	if (IsKnownActivity(Sample.Activity))
	{
		OutBatch.Samples.Add(MakeTransition(
			Sample,
			Sample.Header,
			EOpenMobileActivityTransition::Started
		));
	}
	CommittedSample = MoveTemp(Sample);
	LastTransitionTimestampSeconds =
		CommittedSample.Header.TimestampSeconds;
	bHasTransitionTimestamp = true;
	return !OutBatch.Samples.IsEmpty();
}

void FOpenMobileActivityTransitionTracker::Reset()
{
	CommittedSample = {};
	LastObservedTimestampSeconds = 0.0;
	LastTransitionTimestampSeconds = 0.0;
	bHasCommittedSample = false;
	bHasTransitionTimestamp = false;
}

void FOpenMobileActivityTransitionEventFilter::Configure(
	EOpenMobileActivityConfidence InMinimumConfidence
)
{
	MinimumConfidence = InMinimumConfidence;
	Reset();
}

bool FOpenMobileActivityTransitionEventFilter::Process(
	FOpenMobileActivitySensorSample& Sample
)
{
	FOpenMobileMotionActivityClassifier::NormalizeSample(Sample);
	if (!Sample.Header.bValid
		|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
		|| Sample.Header.TimestampSeconds < 0.0
		|| Sample.Activity == EOpenMobileMotionActivity::Unknown
		|| Sample.Transition == EOpenMobileActivityTransition::None
		|| static_cast<uint8>(Sample.Confidence) <
			static_cast<uint8>(MinimumConfidence))
	{
		return false;
	}
	if (bHasLastEvent
		&& (Sample.Header.TimestampSeconds < LastTimestampSeconds
			|| (Sample.Header.TimestampSeconds == LastTimestampSeconds
				&& Sample.Activity == LastActivity
				&& Sample.Transition == LastTransition)))
	{
		return false;
	}
	const bool bObserved = ObservedActivities.Contains(Sample.Activity);
	const bool bActive = ActiveActivities.Contains(Sample.Activity);
	if ((Sample.Transition == EOpenMobileActivityTransition::Started
			&& bObserved && bActive)
		|| (Sample.Transition == EOpenMobileActivityTransition::Stopped
			&& bObserved && !bActive))
	{
		return false;
	}
	ObservedActivities.Add(Sample.Activity);
	if (Sample.Transition == EOpenMobileActivityTransition::Started)
	{
		ActiveActivities.Add(Sample.Activity);
	}
	else
	{
		ActiveActivities.Remove(Sample.Activity);
	}
	LastActivity = Sample.Activity;
	LastTransition = Sample.Transition;
	LastTimestampSeconds = Sample.Header.TimestampSeconds;
	bHasLastEvent = true;
	return true;
}

void FOpenMobileActivityTransitionEventFilter::Reset()
{
	ObservedActivities.Reset();
	ActiveActivities.Reset();
	LastActivity = EOpenMobileMotionActivity::Unknown;
	LastTransition = EOpenMobileActivityTransition::None;
	LastTimestampSeconds = 0.0;
	bHasLastEvent = false;
}

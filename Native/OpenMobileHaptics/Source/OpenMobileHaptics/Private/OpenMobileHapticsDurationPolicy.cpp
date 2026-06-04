#include "OpenMobileHapticsDurationPolicy.h"

bool FOpenMobileHapticsDurationPolicy::IsWithinBounds(
	double DurationSeconds,
	double MinimumSeconds,
	double MaximumSeconds
)
{
	return FMath::IsFinite(DurationSeconds)
		&& FMath::IsFinite(MinimumSeconds)
		&& FMath::IsFinite(MaximumSeconds)
		&& MinimumSeconds >= 0.0
		&& MaximumSeconds >= MinimumSeconds
		&& DurationSeconds >= MinimumSeconds
		&& DurationSeconds <= MaximumSeconds;
}

FOpenMobileHapticsNativeDurationResolution
FOpenMobileHapticsDurationPolicy::ResolveNativeLimit(
	double DurationSeconds,
	double NativeMaximumSeconds,
	bool bCanSplit
)
{
	FOpenMobileHapticsNativeDurationResolution Resolution;
	if (!FMath::IsFinite(DurationSeconds)
		|| !FMath::IsFinite(NativeMaximumSeconds)
		|| DurationSeconds <= 0.0
		|| NativeMaximumSeconds <= 0.0)
	{
		return Resolution;
	}
	Resolution.RequestedSeconds = DurationSeconds;
	if (DurationSeconds <= NativeMaximumSeconds)
	{
		Resolution.Outcome =
			EOpenMobileHapticsNativeDurationOutcome::Accepted;
		Resolution.NativeSegmentSeconds = DurationSeconds;
		Resolution.SegmentCount = 1;
		return Resolution;
	}
	if (!bCanSplit)
	{
		return Resolution;
	}

	const double RequiredSegments = FMath::CeilToDouble(
		DurationSeconds / NativeMaximumSeconds
	);
	if (!FMath::IsFinite(RequiredSegments)
		|| RequiredSegments > static_cast<double>(MAX_int32))
	{
		return Resolution;
	}
	Resolution.Outcome = EOpenMobileHapticsNativeDurationOutcome::Split;
	Resolution.NativeSegmentSeconds = NativeMaximumSeconds;
	Resolution.SegmentCount = static_cast<int32>(RequiredSegments);
	return Resolution;
}

bool FOpenMobileHapticsDurationPolicy::TryCalculatePatternDuration(
	const FOpenMobileHapticPattern& Pattern,
	double MaximumSeconds,
	double& OutDurationSeconds
)
{
	return TryCalculatePatternDuration(
		Pattern,
		MaximumSeconds,
		MaximumSeconds,
		OutDurationSeconds
	);
}

bool FOpenMobileHapticsDurationPolicy::TryCalculatePatternDuration(
	const FOpenMobileHapticPattern& Pattern,
	double MaximumSeconds,
	double MaximumEventSeconds,
	double& OutDurationSeconds
)
{
	OutDurationSeconds = 0.0;
	if (!FMath::IsFinite(MaximumSeconds)
		|| !FMath::IsFinite(MaximumEventSeconds)
		|| MaximumSeconds < 0.0
		|| MaximumEventSeconds < 0.0
		|| MaximumEventSeconds > MaximumSeconds)
	{
		return false;
	}
	for (const FOpenMobileHapticPatternEvent& Event : Pattern.Events)
	{
		if (!FMath::IsFinite(Event.StartTimeSeconds)
			|| !FMath::IsFinite(Event.DurationSeconds)
			|| Event.StartTimeSeconds < 0.0
			|| Event.DurationSeconds < 0.0
			|| Event.DurationSeconds > MaximumEventSeconds
			|| Event.StartTimeSeconds > MaximumSeconds
			|| Event.DurationSeconds
				> MaximumSeconds - Event.StartTimeSeconds)
		{
			OutDurationSeconds = 0.0;
			return false;
		}
		OutDurationSeconds = FMath::Max(
			OutDurationSeconds,
			Event.StartTimeSeconds + Event.DurationSeconds
		);
	}
	return true;
}

#include "OpenMobileHapticsAndroidPlaybackControlPolicy.h"

namespace OpenMobileHapticsAndroidPlaybackControlPolicyPrivate
{
	/** Builds a control failure without leaving partial waveform arrays for callers to mistake as usable. */
	FOpenMobileHapticsAndroidPlaybackControlResolution Failure(
		EOpenMobileHapticsAndroidPlaybackControlError Error
	)
	{
		FOpenMobileHapticsAndroidPlaybackControlResolution Result;
		Result.Error = Error;
		return Result;
	}

	/** Adds one validated timing and amplitude pair while enforcing the total segment cap before allocation grows. */
	bool AppendSegment(
		FOpenMobileHapticsAndroidPlaybackControlResolution& Result,
		int64 TimingMilliseconds,
		int32 Amplitude,
		int32 MaximumSegmentCount
	)
	{
		if (TimingMilliseconds <= 0
			|| Result.TimingsMilliseconds.Num() >= MaximumSegmentCount)
		{
			return false;
		}
		Result.TimingsMilliseconds.Add(TimingMilliseconds);
		Result.Amplitudes.Add(Amplitude);
		return true;
	}
}

FOpenMobileHapticsAndroidPlaybackControlResolution
FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
	const TArray<int64>& BaseTimingsMilliseconds,
	const TArray<int32>& BaseAmplitudes,
	const FOpenMobileHapticsRepeatPlan& RepeatPlan,
	double PositionSeconds,
	int32 CompletedRepeatCount,
	double RemainingActiveDurationSeconds,
	int32 MaximumSegmentCount
)
{
	using namespace OpenMobileHapticsAndroidPlaybackControlPolicyPrivate;
	const int32 SegmentCount = BaseTimingsMilliseconds.Num();
	if (SegmentCount <= 0 || BaseAmplitudes.Num() != SegmentCount
		|| MaximumSegmentCount <= 0)
	{
		return Failure(
			EOpenMobileHapticsAndroidPlaybackControlError::InvalidWaveform
		);
	}
	int64 PatternDurationMilliseconds = 0;
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const int64 Timing = BaseTimingsMilliseconds[Index];
		const int32 Amplitude = BaseAmplitudes[Index];
		if (Timing <= 0 || PatternDurationMilliseconds > MAX_int64 - Timing
			|| (Amplitude != -1 && (Amplitude < 0 || Amplitude > 255)))
		{
			return Failure(
				EOpenMobileHapticsAndroidPlaybackControlError::InvalidWaveform
			);
		}
		PatternDurationMilliseconds += Timing;
	}
	const int64 PlannedDurationMilliseconds = static_cast<int64>(
		FMath::RoundToDouble(RepeatPlan.PatternDurationSeconds * 1000.0)
	);
	if (!FMath::IsFinite(RepeatPlan.PatternDurationSeconds)
		|| PlannedDurationMilliseconds <= 0
		|| FMath::Abs(
			PatternDurationMilliseconds - PlannedDurationMilliseconds
		) > 1
		|| CompletedRepeatCount < 0
		|| (!RepeatPlan.bRepeatUntilStopped
			&& CompletedRepeatCount > RepeatPlan.RepeatCount))
	{
		return Failure(
			EOpenMobileHapticsAndroidPlaybackControlError::InvalidPlan
		);
	}
	if (!FMath::IsFinite(PositionSeconds) || PositionSeconds < 0.0
		|| PositionSeconds >= RepeatPlan.PatternDurationSeconds)
	{
		return Failure(
			EOpenMobileHapticsAndroidPlaybackControlError::InvalidPosition
		);
	}
	if (!FMath::IsFinite(RemainingActiveDurationSeconds)
		|| RemainingActiveDurationSeconds <= 0.0)
	{
		return Failure(
			EOpenMobileHapticsAndroidPlaybackControlError::InvalidDuration
		);
	}

	const int64 PositionMilliseconds = FMath::Clamp<int64>(
		static_cast<int64>(FMath::RoundToDouble(PositionSeconds * 1000.0)),
		0,
		PatternDurationMilliseconds - 1
	);
	int64 SegmentStartMilliseconds = 0;
	int32 PositionSegmentIndex = INDEX_NONE;
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const int64 SegmentEndMilliseconds = SegmentStartMilliseconds
			+ BaseTimingsMilliseconds[Index];
		if (PositionMilliseconds < SegmentEndMilliseconds)
		{
			PositionSegmentIndex = Index;
			break;
		}
		SegmentStartMilliseconds = SegmentEndMilliseconds;
	}
	if (PositionSegmentIndex == INDEX_NONE)
	{
		return Failure(
			EOpenMobileHapticsAndroidPlaybackControlError::InvalidPosition
		);
	}

	FOpenMobileHapticsAndroidPlaybackControlResolution Result;
	const int64 FirstDurationMilliseconds =
		SegmentStartMilliseconds
		+ BaseTimingsMilliseconds[PositionSegmentIndex]
		- PositionMilliseconds;
	if (!AppendSegment(
		Result,
		FirstDurationMilliseconds,
		BaseAmplitudes[PositionSegmentIndex],
		MaximumSegmentCount
	))
	{
		return Failure(
			EOpenMobileHapticsAndroidPlaybackControlError::SegmentLimit
		);
	}
	for (int32 Index = PositionSegmentIndex + 1;
		Index < SegmentCount;
		++Index)
	{
		if (!AppendSegment(
			Result,
			BaseTimingsMilliseconds[Index],
			BaseAmplitudes[Index],
			MaximumSegmentCount
		))
		{
			return Failure(
				EOpenMobileHapticsAndroidPlaybackControlError::SegmentLimit
			);
		}
	}

	if (RepeatPlan.bLoop)
	{
		const int64 RepeatStartMilliseconds = static_cast<int64>(
			FMath::RoundToDouble(
				RepeatPlan.RepeatStartTimeSeconds * 1000.0
			)
		);
		int64 CumulativeMilliseconds = 0;
		int32 RepeatStartIndex = INDEX_NONE;
		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			if (CumulativeMilliseconds == RepeatStartMilliseconds)
			{
				RepeatStartIndex = Index;
				break;
			}
			CumulativeMilliseconds += BaseTimingsMilliseconds[Index];
		}
		if (RepeatStartIndex == INDEX_NONE)
		{
			return Failure(
				EOpenMobileHapticsAndroidPlaybackControlError::InvalidPlan
			);
		}

		const bool bCanReuseInitialIndefiniteWaveform =
			RepeatPlan.bRepeatUntilStopped
			&& PositionMilliseconds == 0
			&& CompletedRepeatCount == 0;
		const int32 AdditionalRepeatCount =
			bCanReuseInitialIndefiniteWaveform
				? 0
				: RepeatPlan.bRepeatUntilStopped
					? 1
					: RepeatPlan.RepeatCount - CompletedRepeatCount;
		if (bCanReuseInitialIndefiniteWaveform)
		{
			Result.RepeatIndex = RepeatStartIndex;
		}
		else if (RepeatPlan.bRepeatUntilStopped)
		{
			Result.RepeatIndex = Result.TimingsMilliseconds.Num();
		}
		for (int32 Repeat = 0; Repeat < AdditionalRepeatCount; ++Repeat)
		{
			for (int32 Index = RepeatStartIndex;
				Index < SegmentCount;
				++Index)
			{
				if (!AppendSegment(
					Result,
					BaseTimingsMilliseconds[Index],
					BaseAmplitudes[Index],
					MaximumSegmentCount
				))
				{
					return Failure(
						EOpenMobileHapticsAndroidPlaybackControlError::
							SegmentLimit
					);
				}
			}
		}
	}

	const int64 RemainingActiveDurationMilliseconds = static_cast<int64>(
		FMath::CeilToDouble(RemainingActiveDurationSeconds * 1000.0)
	);
	int64 FiniteDurationMilliseconds = 0;
	for (const int64 Timing : Result.TimingsMilliseconds)
	{
		if (FiniteDurationMilliseconds > MAX_int64 - Timing)
		{
			return Failure(
				EOpenMobileHapticsAndroidPlaybackControlError::InvalidDuration
			);
		}
		FiniteDurationMilliseconds += Timing;
	}
	Result.CompletionDurationMilliseconds = RepeatPlan.bRepeatUntilStopped
		? RemainingActiveDurationMilliseconds
		: FMath::Min(
			FiniteDurationMilliseconds,
			RemainingActiveDurationMilliseconds
		);
	if (Result.CompletionDurationMilliseconds <= 0)
	{
		return Failure(
			EOpenMobileHapticsAndroidPlaybackControlError::InvalidDuration
		);
	}
	Result.Error = EOpenMobileHapticsAndroidPlaybackControlError::None;
	return Result;
}

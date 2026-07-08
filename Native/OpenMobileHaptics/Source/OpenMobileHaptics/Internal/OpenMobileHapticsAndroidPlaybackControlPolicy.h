#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsRepeatPolicy.h"

enum class EOpenMobileHapticsAndroidPlaybackControlError : uint8
{
	None,
	InvalidWaveform,
	InvalidPlan,
	InvalidPosition,
	InvalidDuration,
	SegmentLimit
};

struct FOpenMobileHapticsAndroidPlaybackControlResolution
{
	TArray<int64> TimingsMilliseconds;
	TArray<int32> Amplitudes;
	int32 RepeatIndex = INDEX_NONE;
	int64 CompletionDurationMilliseconds = 0;
	EOpenMobileHapticsAndroidPlaybackControlError Error =
		EOpenMobileHapticsAndroidPlaybackControlError::InvalidWaveform;

	bool IsSuccess() const
	{
		return Error == EOpenMobileHapticsAndroidPlaybackControlError::None;
	}
};

class FOpenMobileHapticsAndroidPlaybackControlPolicy final
{
public:
	static FOpenMobileHapticsAndroidPlaybackControlResolution Resolve(
		const TArray<int64>& BaseTimingsMilliseconds,
		const TArray<int32>& BaseAmplitudes,
		const FOpenMobileHapticsRepeatPlan& RepeatPlan,
		double PositionSeconds,
		int32 CompletedRepeatCount,
		double RemainingActiveDurationSeconds,
		int32 MaximumSegmentCount
	);
};

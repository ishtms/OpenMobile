#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsRepeatError : uint8
{
	None,
	InvalidPatternDuration,
	InvalidRepeatStart,
	RepeatLimit,
	InvalidSafetyDuration,
	DurationLimit
};

struct FOpenMobileHapticsRepeatPlan
{
	bool bLoop = false;
	bool bRepeatUntilStopped = false;
	int32 RepeatCount = 0;
	int32 TotalIterationCount = 1;
	double PatternDurationSeconds = 0.0;
	double RepeatStartTimeSeconds = 0.0;
	double RepeatDurationSeconds = 0.0;
	double TotalDurationSeconds = 0.0;
	double MaximumDurationSeconds = 0.0;
};

struct FOpenMobileHapticsRepeatPlanResult
{
	FOpenMobileHapticsRepeatPlan Plan;
	EOpenMobileHapticsRepeatError Error = EOpenMobileHapticsRepeatError::None;

	bool IsSuccess() const
	{
		return Error == EOpenMobileHapticsRepeatError::None;
	}
};

struct FOpenMobileHapticsRepeatAdvance
{
	bool bShouldSubmit = false;
	int32 IterationIndex = 0;
	double ScheduledStartSeconds = 0.0;
};

class FOpenMobileHapticsRepeatPolicy final
{
public:
	static FOpenMobileHapticsRepeatPlanResult Resolve(
		const FOpenMobileHapticLoopOptions& Options,
		double PatternDurationSeconds,
		int32 MaximumFiniteRepeatCount,
		double GlobalMaximumDurationSeconds
	);
};

class FOpenMobileHapticsRepeatCursor final
{
public:
	FOpenMobileHapticsRepeatCursor(
		const FOpenMobileHapticsRepeatPlan& InPlan,
		FOpenMobileHapticPlaybackHandle InHandle,
		double InStartTimeSeconds
	);

	FOpenMobileHapticsRepeatAdvance Advance(double CurrentTimeSeconds);
	bool Stop(FOpenMobileHapticPlaybackHandle Handle);
	void Cancel();

private:
	FOpenMobileHapticsRepeatPlan Plan;
	FOpenMobileHapticPlaybackHandle OwnerHandle;
	double StartTimeSeconds = 0.0;
	int32 LastSubmittedIteration = 0;
	bool bStopped = false;
};

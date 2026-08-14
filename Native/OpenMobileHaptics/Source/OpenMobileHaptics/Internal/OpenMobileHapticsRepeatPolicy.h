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

	/** Requires the plan and its validation result to be read together, a default-looking plan can still represent failure. */
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
	/** Resolves finite and until-stopped loops into one bounded schedule before any backend work begins. */
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
	/** Captures the accepted owner and start clock so later advances can't be redirected to another playback. */
	FOpenMobileHapticsRepeatCursor(
		const FOpenMobileHapticsRepeatPlan& InPlan,
		FOpenMobileHapticPlaybackHandle InHandle,
		double InStartTimeSeconds
	);

	/** Emits at most the next due iteration, which prevents a late tick from flooding native submissions. */
	FOpenMobileHapticsRepeatAdvance Advance(double CurrentTimeSeconds);
	/** Stops only for the owning handle, stale handles shouldn't cancel a newer cursor. */
	bool Stop(FOpenMobileHapticPlaybackHandle Handle);
	/** Cancels future advances without requiring a handle during subsystem teardown. */
	void Cancel();

private:
	FOpenMobileHapticsRepeatPlan Plan;
	FOpenMobileHapticPlaybackHandle OwnerHandle;
	double StartTimeSeconds = 0.0;
	int32 LastSubmittedIteration = 0;
	bool bStopped = false;
};

#include "OpenMobileHapticsRepeatPolicy.h"

namespace OpenMobileHapticsRepeatPolicyPrivate
{
	/** Returns the exact repeat error with a default plan callers won't submit accidentally. */
	FOpenMobileHapticsRepeatPlanResult Failure(
		EOpenMobileHapticsRepeatError Error
	)
	{
		FOpenMobileHapticsRepeatPlanResult Result;
		Result.Error = Error;
		return Result;
	}
}

FOpenMobileHapticsRepeatPlanResult FOpenMobileHapticsRepeatPolicy::Resolve(
	const FOpenMobileHapticLoopOptions& Options,
	double PatternDurationSeconds,
	int32 MaximumFiniteRepeatCount,
	double GlobalMaximumDurationSeconds
)
{
	using namespace OpenMobileHapticsRepeatPolicyPrivate;
	if (!FMath::IsFinite(PatternDurationSeconds)
		|| PatternDurationSeconds <= 0.0)
	{
		return Failure(EOpenMobileHapticsRepeatError::InvalidPatternDuration);
	}
	if (!FMath::IsFinite(GlobalMaximumDurationSeconds)
		|| GlobalMaximumDurationSeconds <= 0.0
		|| MaximumFiniteRepeatCount < 1)
	{
		return Failure(EOpenMobileHapticsRepeatError::InvalidSafetyDuration);
	}

	FOpenMobileHapticsRepeatPlanResult Result;
	Result.Plan.PatternDurationSeconds = PatternDurationSeconds;
	Result.Plan.TotalDurationSeconds = PatternDurationSeconds;
	Result.Plan.MaximumDurationSeconds = GlobalMaximumDurationSeconds;
	if (!Options.bLoop)
	{
		if (PatternDurationSeconds > GlobalMaximumDurationSeconds)
		{
			return Failure(EOpenMobileHapticsRepeatError::DurationLimit);
		}
		return Result;
	}
	if (Options.RepeatCount < 0
		|| Options.RepeatCount > MaximumFiniteRepeatCount)
	{
		return Failure(EOpenMobileHapticsRepeatError::RepeatLimit);
	}
	if (!FMath::IsFinite(Options.RepeatStartTimeSeconds)
		|| Options.RepeatStartTimeSeconds < 0.0
		|| Options.RepeatStartTimeSeconds >= PatternDurationSeconds)
	{
		return Failure(EOpenMobileHapticsRepeatError::InvalidRepeatStart);
	}
	if (!FMath::IsFinite(Options.MaximumDurationSeconds)
		|| Options.MaximumDurationSeconds <= 0.0
		|| Options.MaximumDurationSeconds > GlobalMaximumDurationSeconds)
	{
		return Failure(EOpenMobileHapticsRepeatError::InvalidSafetyDuration);
	}

	Result.Plan.bLoop = true;
	Result.Plan.bRepeatUntilStopped = Options.RepeatCount == 0;
	Result.Plan.RepeatCount = Options.RepeatCount;
	Result.Plan.TotalIterationCount = Result.Plan.bRepeatUntilStopped
		? 0
		: Options.RepeatCount + 1;
	Result.Plan.RepeatStartTimeSeconds = Options.RepeatStartTimeSeconds;
	Result.Plan.RepeatDurationSeconds =
		PatternDurationSeconds - Options.RepeatStartTimeSeconds;
	Result.Plan.MaximumDurationSeconds = Options.MaximumDurationSeconds;
	if (Result.Plan.bRepeatUntilStopped)
	{
		Result.Plan.TotalDurationSeconds = Options.MaximumDurationSeconds;
		return Result;
	}
	if (PatternDurationSeconds > Options.MaximumDurationSeconds
		|| Result.Plan.RepeatDurationSeconds > 0.0
			&& static_cast<double>(Options.RepeatCount)
				> (Options.MaximumDurationSeconds - PatternDurationSeconds)
					/ Result.Plan.RepeatDurationSeconds)
	{
		return Failure(EOpenMobileHapticsRepeatError::DurationLimit);
	}
	Result.Plan.TotalDurationSeconds = PatternDurationSeconds
		+ static_cast<double>(Options.RepeatCount)
			* Result.Plan.RepeatDurationSeconds;
	return Result;
}

FOpenMobileHapticsRepeatCursor::FOpenMobileHapticsRepeatCursor(
	const FOpenMobileHapticsRepeatPlan& InPlan,
	FOpenMobileHapticPlaybackHandle InHandle,
	double InStartTimeSeconds
)
	: Plan(InPlan)
	, OwnerHandle(InHandle)
	, StartTimeSeconds(InStartTimeSeconds)
{
}

FOpenMobileHapticsRepeatAdvance FOpenMobileHapticsRepeatCursor::Advance(
	double CurrentTimeSeconds
)
{
	FOpenMobileHapticsRepeatAdvance Advance;
	if (bStopped
		|| !Plan.bLoop
		|| !OwnerHandle.IsValid()
		|| !FMath::IsFinite(StartTimeSeconds)
		|| !FMath::IsFinite(CurrentTimeSeconds)
		|| Plan.RepeatDurationSeconds <= 0.0
		|| CurrentTimeSeconds < StartTimeSeconds
		|| CurrentTimeSeconds
			>= StartTimeSeconds + Plan.MaximumDurationSeconds)
	{
		return Advance;
	}
	const double FirstRepeatTime =
		StartTimeSeconds + Plan.PatternDurationSeconds;
	if (CurrentTimeSeconds < FirstRepeatTime)
	{
		return Advance;
	}
	const double DueIndexDouble = FMath::FloorToDouble(
		(CurrentTimeSeconds - FirstRepeatTime)
			/ Plan.RepeatDurationSeconds
		+ UE_DOUBLE_SMALL_NUMBER
	) + 1.0;
	if (!FMath::IsFinite(DueIndexDouble)
		|| DueIndexDouble > static_cast<double>(MAX_int32))
	{
		return Advance;
	}
	const int32 DueIndex = static_cast<int32>(DueIndexDouble);
	if (DueIndex <= LastSubmittedIteration
		|| (!Plan.bRepeatUntilStopped && DueIndex > Plan.RepeatCount))
	{
		return Advance;
	}
	LastSubmittedIteration = DueIndex;
	Advance.bShouldSubmit = true;
	Advance.IterationIndex = DueIndex;
	Advance.ScheduledStartSeconds = FirstRepeatTime
		+ static_cast<double>(DueIndex - 1) * Plan.RepeatDurationSeconds;
	return Advance;
}

bool FOpenMobileHapticsRepeatCursor::Stop(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	if (bStopped || !Handle.IsValid() || Handle != OwnerHandle)
	{
		return false;
	}
	bStopped = true;
	return true;
}

void FOpenMobileHapticsRepeatCursor::Cancel()
{
	bStopped = true;
}

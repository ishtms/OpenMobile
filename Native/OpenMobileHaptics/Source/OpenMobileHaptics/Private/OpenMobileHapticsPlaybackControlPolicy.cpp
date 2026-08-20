#include "OpenMobileHapticsPlaybackControlPolicy.h"

namespace OpenMobileHapticsPlaybackControlPolicyPrivate
{
	constexpr double PositionToleranceSeconds = 1.0e-9;

	/** Treats every public terminal state alike so pause, resume, and seek reject them consistently. */
	bool IsTerminalState(EOpenMobileHapticPlaybackState State)
	{
		switch (State)
		{
		case EOpenMobileHapticPlaybackState::Stopped:
		case EOpenMobileHapticPlaybackState::Cancelled:
		case EOpenMobileHapticPlaybackState::Completed:
		case EOpenMobileHapticPlaybackState::Interrupted:
		case EOpenMobileHapticPlaybackState::Failed:
			return true;
		default:
			return false;
		}
	}

	/** Allows control tracking to start only after acceptance or scheduling, never from an invalid or completed state. */
	bool IsInitialState(EOpenMobileHapticPlaybackState State)
	{
		return State == EOpenMobileHapticPlaybackState::Accepted
			|| State == EOpenMobileHapticPlaybackState::Started
			|| State == EOpenMobileHapticPlaybackState::Paused
			|| State == EOpenMobileHapticPlaybackState::Resumed;
	}
}

FOpenMobileHapticsPlaybackControlPolicy::
FOpenMobileHapticsPlaybackControlPolicy(
	const FOpenMobileHapticsRepeatPlan& InPlan,
	EOpenMobileHapticPlaybackState InState,
	double InStartTimeSeconds
)
	: Plan(InPlan)
	, State(InState)
	, LastUpdateTimeSeconds(InStartTimeSeconds)
{
	using namespace OpenMobileHapticsPlaybackControlPolicyPrivate;
	bValid = IsInitialState(State)
		&& FMath::IsFinite(InStartTimeSeconds)
		&& FMath::IsFinite(Plan.PatternDurationSeconds)
		&& Plan.PatternDurationSeconds > 0.0
		&& FMath::IsFinite(Plan.MaximumDurationSeconds)
		&& Plan.MaximumDurationSeconds > 0.0
		&& (!Plan.bLoop
			|| (FMath::IsFinite(Plan.RepeatStartTimeSeconds)
				&& Plan.RepeatStartTimeSeconds >= 0.0
				&& Plan.RepeatStartTimeSeconds
					< Plan.PatternDurationSeconds
				&& FMath::IsFinite(Plan.RepeatDurationSeconds)
				&& Plan.RepeatDurationSeconds > 0.0
				&& (Plan.bRepeatUntilStopped || Plan.RepeatCount > 0)));
	if (!bValid)
	{
		State = EOpenMobileHapticPlaybackState::Invalid;
	}
}

bool FOpenMobileHapticsPlaybackControlPolicy::IsActive() const
{
	return State == EOpenMobileHapticPlaybackState::Accepted
		|| State == EOpenMobileHapticPlaybackState::Started
		|| State == EOpenMobileHapticPlaybackState::Resumed;
}

bool FOpenMobileHapticsPlaybackControlPolicy::IsTerminal() const
{
	return OpenMobileHapticsPlaybackControlPolicyPrivate::IsTerminalState(
		State
	);
}

void FOpenMobileHapticsPlaybackControlPolicy::AdvanceTimeline(
	double DeltaSeconds
)
{
	using namespace OpenMobileHapticsPlaybackControlPolicyPrivate;
	if (DeltaSeconds <= PositionToleranceSeconds || IsTerminal())
	{
		return;
	}

	while (DeltaSeconds > PositionToleranceSeconds)
	{
		const double ToPatternEnd = FMath::Max(
			0.0,
			Plan.PatternDurationSeconds - TimelinePositionSeconds
		);
		if (DeltaSeconds + PositionToleranceSeconds < ToPatternEnd)
		{
			TimelinePositionSeconds += DeltaSeconds;
			return;
		}

		DeltaSeconds = FMath::Max(0.0, DeltaSeconds - ToPatternEnd);
		const bool bCanRepeat = Plan.bLoop
			&& (Plan.bRepeatUntilStopped
				|| CompletedRepeatCount < Plan.RepeatCount);
		if (!bCanRepeat)
		{
			TimelinePositionSeconds = Plan.PatternDurationSeconds;
			State = EOpenMobileHapticPlaybackState::Completed;
			return;
		}

		TimelinePositionSeconds = Plan.RepeatStartTimeSeconds;
		++CompletedRepeatCount;
		if (DeltaSeconds <= PositionToleranceSeconds)
		{
			return;
		}

		const double CompleteRepeatCount = FMath::FloorToDouble(
			DeltaSeconds / Plan.RepeatDurationSeconds
		);
		const int32 RemainingFiniteRepeats = Plan.bRepeatUntilStopped
			? MAX_int32
			: Plan.RepeatCount - CompletedRepeatCount;
		const int32 SkippedRepeats = FMath::Clamp<int32>(
			static_cast<int32>(FMath::Min(
				CompleteRepeatCount,
				static_cast<double>(MAX_int32)
			)),
			0,
			RemainingFiniteRepeats
		);
		if (SkippedRepeats > 0)
		{
			CompletedRepeatCount += SkippedRepeats;
			DeltaSeconds -= static_cast<double>(SkippedRepeats)
				* Plan.RepeatDurationSeconds;
		}
	}
}

void FOpenMobileHapticsPlaybackControlPolicy::Advance(double NowSeconds)
{
	using namespace OpenMobileHapticsPlaybackControlPolicyPrivate;
	if (!bValid || !IsActive() || !FMath::IsFinite(NowSeconds)
		|| NowSeconds < LastUpdateTimeSeconds)
	{
		return;
	}

	const double DeltaSeconds = NowSeconds - LastUpdateTimeSeconds;
	LastUpdateTimeSeconds = NowSeconds;
	const double RemainingSafetyDuration = FMath::Max(
		0.0,
		Plan.MaximumDurationSeconds - ActiveDurationSeconds
	);
	const double EffectiveDelta = FMath::Min(
		DeltaSeconds,
		RemainingSafetyDuration
	);
	ActiveDurationSeconds += EffectiveDelta;
	AdvanceTimeline(EffectiveDelta);
	if (!IsTerminal()
		&& RemainingSafetyDuration <= DeltaSeconds + PositionToleranceSeconds)
	{
		State = EOpenMobileHapticPlaybackState::Completed;
	}
}

FOpenMobileHapticsPlaybackControlSnapshot
FOpenMobileHapticsPlaybackControlPolicy::Snapshot(double NowSeconds)
{
	Advance(NowSeconds);
	FOpenMobileHapticsPlaybackControlSnapshot Snapshot;
	Snapshot.State = State;
	Snapshot.TimelinePositionSeconds = TimelinePositionSeconds;
	Snapshot.CompletedRepeatCount = CompletedRepeatCount;
	Snapshot.ActiveDurationSeconds = ActiveDurationSeconds;
	Snapshot.Revision = Revision;
	if (bValid && !IsTerminal())
	{
		const double SafetyRemaining = FMath::Max(0.0,
			Plan.MaximumDurationSeconds - ActiveDurationSeconds);
		const double RepeatsRemaining = Plan.bLoop
			? FMath::Max(0, Plan.RepeatCount - CompletedRepeatCount) * Plan.RepeatDurationSeconds
			: 0.0;
		Snapshot.RemainingDurationSeconds = Plan.bRepeatUntilStopped
			? SafetyRemaining
			: FMath::Min(SafetyRemaining, FMath::Max(0.0,
				Plan.PatternDurationSeconds - TimelinePositionSeconds + RepeatsRemaining));
	}
	return Snapshot;
}

FOpenMobileHapticsPlaybackControlTransition
FOpenMobileHapticsPlaybackControlPolicy::MakeTransition(
	EOpenMobileHapticsPlaybackControlTransitionOutcome Outcome,
	double RequestedPositionSeconds,
	bool bQuantized
) const
{
	FOpenMobileHapticsPlaybackControlTransition Transition;
	Transition.Outcome = Outcome;
	Transition.State = State;
	Transition.RequestedPositionSeconds = RequestedPositionSeconds;
	Transition.ResolvedPositionSeconds = TimelinePositionSeconds;
	Transition.ActiveDurationSeconds = ActiveDurationSeconds;
	Transition.CompletedRepeatCount = CompletedRepeatCount;
	Transition.Revision = Revision;
	Transition.bQuantized = bQuantized;
	return Transition;
}

FOpenMobileHapticsPlaybackControlTransition
FOpenMobileHapticsPlaybackControlPolicy::Pause(double NowSeconds)
{
	if (!bValid || !IsActive())
	{
		return MakeTransition(
			EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState
		);
	}
	Advance(NowSeconds);
	if (IsTerminal())
	{
		return MakeTransition(
			EOpenMobileHapticsPlaybackControlTransitionOutcome::Finished
		);
	}
	if (!FMath::IsFinite(NowSeconds) || NowSeconds < LastUpdateTimeSeconds)
	{
		return MakeTransition(
			EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState
		);
	}
	State = EOpenMobileHapticPlaybackState::Paused;
	LastUpdateTimeSeconds = NowSeconds;
	++Revision;
	return MakeTransition(
		EOpenMobileHapticsPlaybackControlTransitionOutcome::Accepted
	);
}

FOpenMobileHapticsPlaybackControlTransition
FOpenMobileHapticsPlaybackControlPolicy::Resume(double NowSeconds)
{
	if (!bValid || State != EOpenMobileHapticPlaybackState::Paused
		|| !FMath::IsFinite(NowSeconds)
		|| NowSeconds < LastUpdateTimeSeconds)
	{
		return MakeTransition(
			EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState
		);
	}
	State = EOpenMobileHapticPlaybackState::Resumed;
	LastUpdateTimeSeconds = NowSeconds;
	++Revision;
	return MakeTransition(
		EOpenMobileHapticsPlaybackControlTransitionOutcome::Accepted
	);
}

FOpenMobileHapticsPlaybackControlTransition
FOpenMobileHapticsPlaybackControlPolicy::Seek(
	double PositionSeconds,
	double NowSeconds,
	double GranularitySeconds
)
{
	using namespace OpenMobileHapticsPlaybackControlPolicyPrivate;
	if (!bValid || (!IsActive()
		&& State != EOpenMobileHapticPlaybackState::Paused))
	{
		return MakeTransition(
			EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState,
			PositionSeconds
		);
	}
	if (!FMath::IsFinite(PositionSeconds) || PositionSeconds < 0.0
		|| PositionSeconds >= Plan.PatternDurationSeconds
		|| !FMath::IsFinite(GranularitySeconds)
		|| GranularitySeconds < 0.0
		|| !FMath::IsFinite(NowSeconds)
		|| NowSeconds < LastUpdateTimeSeconds)
	{
		return MakeTransition(
			EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidPosition,
			PositionSeconds
		);
	}
	Advance(NowSeconds);
	if (IsTerminal())
	{
		return MakeTransition(
			EOpenMobileHapticsPlaybackControlTransitionOutcome::Finished,
			PositionSeconds
		);
	}

	double ResolvedPosition = PositionSeconds;
	if (GranularitySeconds > PositionToleranceSeconds)
	{
		ResolvedPosition = FMath::FloorToDouble(
			(PositionSeconds + PositionToleranceSeconds) / GranularitySeconds
		) * GranularitySeconds;
	}
	ResolvedPosition = FMath::Clamp(
		ResolvedPosition,
		0.0,
		Plan.PatternDurationSeconds - PositionToleranceSeconds
	);
	const bool bQuantized = !FMath::IsNearlyEqual(
		PositionSeconds,
		ResolvedPosition,
		PositionToleranceSeconds
	);
	TimelinePositionSeconds = ResolvedPosition;
	LastUpdateTimeSeconds = NowSeconds;
	++Revision;
	return MakeTransition(
		EOpenMobileHapticsPlaybackControlTransitionOutcome::Accepted,
		PositionSeconds,
		bQuantized
	);
}

void FOpenMobileHapticsPlaybackControlPolicy::MarkTerminal(
	EOpenMobileHapticPlaybackState TerminalState
)
{
	if (OpenMobileHapticsPlaybackControlPolicyPrivate::IsTerminalState(
		TerminalState
	))
	{
		State = TerminalState;
	}
}

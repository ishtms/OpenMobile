#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsRepeatPolicy.h"

enum class EOpenMobileHapticsPlaybackControlTransitionOutcome : uint8
{
	Accepted,
	InvalidState,
	InvalidPosition,
	Finished
};

struct FOpenMobileHapticsPlaybackControlSnapshot
{
	double RemainingDurationSeconds = 0.0;
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;
	double TimelinePositionSeconds = 0.0;
	int32 CompletedRepeatCount = 0;
	double ActiveDurationSeconds = 0.0;
	uint64 Revision = 0;
};

struct FOpenMobileHapticsPlaybackControlTransition
{
	EOpenMobileHapticsPlaybackControlTransitionOutcome Outcome =
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState;
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;
	double RequestedPositionSeconds = 0.0;
	double ResolvedPositionSeconds = 0.0;
	double ActiveDurationSeconds = 0.0;
	int32 CompletedRepeatCount = 0;
	uint64 Revision = 0;
	bool bQuantized = false;
};

class FOpenMobileHapticsPlaybackControlPolicy final
{
public:
	/** Starts control state from an accepted repeat plan and clock, invalid plans stay unusable instead of guessing. */
	FOpenMobileHapticsPlaybackControlPolicy(
		const FOpenMobileHapticsRepeatPlan& InPlan,
		EOpenMobileHapticPlaybackState InState,
		double InStartTimeSeconds
	);

	/** Tells callers whether construction produced a controllable timeline. */
	bool IsValid() const { return bValid; }
	/** Changes whenever a control transition is accepted, letting async callbacks reject stale revisions. */
	uint64 GetRevision() const { return Revision; }
	/** Advances to the supplied clock and returns a coherent state copy for the public playback object. */
	FOpenMobileHapticsPlaybackControlSnapshot Snapshot(double NowSeconds);
	/** Freezes active time at the accepted playhead, repeated pauses don't move it again. */
	FOpenMobileHapticsPlaybackControlTransition Pause(double NowSeconds);
	/** Resumes from the stored playhead only while paused, terminal playback can't restart. */
	FOpenMobileHapticsPlaybackControlTransition Resume(double NowSeconds);
	/** Moves to a validated playhead and quantizes only when the backend requires a seek step. */
	FOpenMobileHapticsPlaybackControlTransition Seek(
		double PositionSeconds,
		double NowSeconds,
		double GranularitySeconds
	);
	/** Seals the state after completion, failure, or cancellation so late controls get rejected consistently. */
	void MarkTerminal(EOpenMobileHapticPlaybackState TerminalState);

private:
	/** Treats only playing and paused state as controllable, queued work hasn't reached this policy yet. */
	bool IsActive() const;
	/** Keeps all terminal checks in one place so callbacks and controls agree. */
	bool IsTerminal() const;
	/** Advances elapsed active time from a monotonic caller clock and ignores invalid deltas. */
	void Advance(double NowSeconds);
	/** Folds elapsed time through repeat segments without losing completed iteration count. */
	void AdvanceTimeline(double DeltaSeconds);
	/** Packages the current state and revision after each transition, including rejected attempts. */
	FOpenMobileHapticsPlaybackControlTransition MakeTransition(
		EOpenMobileHapticsPlaybackControlTransitionOutcome Outcome,
		double RequestedPositionSeconds = 0.0,
		bool bQuantized = false
	) const;

	FOpenMobileHapticsRepeatPlan Plan;
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;
	double TimelinePositionSeconds = 0.0;
	double ActiveDurationSeconds = 0.0;
	double LastUpdateTimeSeconds = 0.0;
	int32 CompletedRepeatCount = 0;
	uint64 Revision = 0;
	bool bValid = false;
};

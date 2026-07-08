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
	FOpenMobileHapticsPlaybackControlPolicy(
		const FOpenMobileHapticsRepeatPlan& InPlan,
		EOpenMobileHapticPlaybackState InState,
		double InStartTimeSeconds
	);

	bool IsValid() const { return bValid; }
	uint64 GetRevision() const { return Revision; }
	FOpenMobileHapticsPlaybackControlSnapshot Snapshot(double NowSeconds);
	FOpenMobileHapticsPlaybackControlTransition Pause(double NowSeconds);
	FOpenMobileHapticsPlaybackControlTransition Resume(double NowSeconds);
	FOpenMobileHapticsPlaybackControlTransition Seek(
		double PositionSeconds,
		double NowSeconds,
		double GranularitySeconds
	);
	void MarkTerminal(EOpenMobileHapticPlaybackState TerminalState);

private:
	bool IsActive() const;
	bool IsTerminal() const;
	void Advance(double NowSeconds);
	void AdvanceTimeline(double DeltaSeconds);
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

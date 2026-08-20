#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsPlaybackControlPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackControlStateTest,
	"OpenMobile.Haptics.Playback.Controls.StateAndPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackControlStateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsRepeatPlan Plan;
	Plan.bLoop = true;
	Plan.RepeatCount = 2;
	Plan.TotalIterationCount = 3;
	Plan.PatternDurationSeconds = 2.0;
	Plan.RepeatStartTimeSeconds = 0.5;
	Plan.RepeatDurationSeconds = 1.5;
	Plan.TotalDurationSeconds = 5.0;
	Plan.MaximumDurationSeconds = 5.0;

	FOpenMobileHapticsPlaybackControlPolicy Policy(
		Plan,
		EOpenMobileHapticPlaybackState::Accepted,
		10.0
	);
	TestTrue(TEXT("Valid repeat plans create a control cursor"),
		Policy.IsValid());
	TestEqual(TEXT("Active playback cannot resume"),
		Policy.Resume(10.0).Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState);

	const FOpenMobileHapticsPlaybackControlTransition Paused =
		Policy.Pause(10.75);
	TestEqual(TEXT("Active playback can pause"), Paused.Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::Accepted);
	TestEqual(TEXT("Pause records the paused state"), Paused.State,
		EOpenMobileHapticPlaybackState::Paused);
	TestEqual(TEXT("Pause captures the timeline position"),
		Paused.ResolvedPositionSeconds, 0.75);
	TestEqual(TEXT("First accepted control owns revision one"),
		Paused.Revision, static_cast<uint64>(1));
	TestEqual(TEXT("Repeated pause is rejected"),
		Policy.Pause(11.0).Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState);

	const FOpenMobileHapticsPlaybackControlTransition Resumed =
		Policy.Resume(20.0);
	TestEqual(TEXT("Paused playback can resume"), Resumed.Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::Accepted);
	TestEqual(TEXT("Resume records the resumed state"), Resumed.State,
		EOpenMobileHapticPlaybackState::Resumed);
	TestEqual(TEXT("Background pause time does not advance position"),
		Resumed.ResolvedPositionSeconds, 0.75);
	TestEqual(TEXT("Resume advances the serialized revision"),
		Resumed.Revision, static_cast<uint64>(2));

	const FOpenMobileHapticsPlaybackControlSnapshot AfterResume =
		Policy.Snapshot(20.25);
	TestEqual(TEXT("Active time after resume advances position"),
		AfterResume.TimelinePositionSeconds, 1.0);
	TestEqual(TEXT("Initial iteration has not looped"),
		AfterResume.CompletedRepeatCount, 0);

	const FOpenMobileHapticsPlaybackControlTransition Sought =
		Policy.Seek(1.2344, 20.25, 0.001);
	TestEqual(TEXT("Finite seek positions are accepted"), Sought.Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::Accepted);
	TestTrue(TEXT("Sub-millisecond seek reports quantization"),
		Sought.bQuantized);
	TestEqual(TEXT("Seek rounds to native milliseconds"),
		Sought.ResolvedPositionSeconds, 1.234);
	TestEqual(TEXT("Seek preserves the current playback state"), Sought.State,
		EOpenMobileHapticPlaybackState::Resumed);
	TestEqual(TEXT("Seek advances the serialized revision"),
		Sought.Revision, static_cast<uint64>(3));

	TestEqual(TEXT("Negative seek is rejected"),
		Policy.Seek(-0.001, 20.25, 0.001).Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidPosition);
	TestEqual(TEXT("Pattern-end seek is rejected"),
		Policy.Seek(2.0, 20.25, 0.001).Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidPosition);
	TestEqual(TEXT("Nonfinite seek is rejected"),
		Policy.Seek(
			std::numeric_limits<double>::quiet_NaN(),
			20.25,
			0.001
		).Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidPosition);
	TestEqual(TEXT("Rejected controls do not consume revisions"),
		Policy.GetRevision(), static_cast<uint64>(3));

	FOpenMobileHapticsPlaybackControlPolicy PausedSeekPolicy(
		Plan,
		EOpenMobileHapticPlaybackState::Started,
		30.0
	);
	PausedSeekPolicy.Pause(30.25);
	const FOpenMobileHapticsPlaybackControlTransition PausedSeek =
		PausedSeekPolicy.Seek(1.5, 40.0, 0.001);
	TestEqual(TEXT("Paused playback can seek"), PausedSeek.Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::Accepted);
	TestEqual(TEXT("Paused seek does not resume playback"), PausedSeek.State,
		EOpenMobileHapticPlaybackState::Paused);
	PausedSeekPolicy.Resume(50.0);
	TestEqual(TEXT("Resume uses the paused seek position"),
		PausedSeekPolicy.Snapshot(50.25).TimelinePositionSeconds,
		1.75);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackControlLoopTest,
	"OpenMobile.Haptics.Playback.Controls.LoopBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackControlLoopTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsRepeatPlan Plan;
	Plan.bLoop = true;
	Plan.bRepeatUntilStopped = true;
	Plan.PatternDurationSeconds = 2.0;
	Plan.RepeatStartTimeSeconds = 0.5;
	Plan.RepeatDurationSeconds = 1.5;
	Plan.TotalDurationSeconds = 30.0;
	Plan.MaximumDurationSeconds = 30.0;

	FOpenMobileHapticsPlaybackControlPolicy Policy(
		Plan,
		EOpenMobileHapticPlaybackState::Started,
		5.0
	);
	const FOpenMobileHapticsPlaybackControlSnapshot FirstBoundary =
		Policy.Snapshot(7.0);
	TestEqual(TEXT("Exact pattern end wraps to the repeat start"),
		FirstBoundary.TimelinePositionSeconds, 0.5);
	TestEqual(TEXT("First boundary starts repeat one"),
		FirstBoundary.CompletedRepeatCount, 1);

	const FOpenMobileHapticsPlaybackControlTransition Paused =
		Policy.Pause(8.75);
	TestEqual(TEXT("Later loop boundaries preserve the loop cursor"),
		Paused.ResolvedPositionSeconds, 0.75);
	TestEqual(TEXT("Completed repeat count is retained"),
		Paused.CompletedRepeatCount, 2);
	Policy.Resume(50.0);
	const FOpenMobileHapticsPlaybackControlSnapshot Resumed =
		Policy.Snapshot(50.25);
	TestEqual(TEXT("Paused wall time does not alter loop count"),
		Resumed.CompletedRepeatCount, 2);
	TestEqual(TEXT("Resume continues from the paused loop position"),
		Resumed.TimelinePositionSeconds, 1.0);

	Policy.MarkTerminal(EOpenMobileHapticPlaybackState::Interrupted);
	TestEqual(TEXT("Terminal playback cannot be paused"),
		Policy.Pause(51.0).Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState);
	TestEqual(TEXT("Terminal playback cannot resume"),
		Policy.Resume(51.0).Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState);
	TestEqual(TEXT("Terminal playback cannot seek"),
		Policy.Seek(0.5, 51.0, 0.001).Outcome,
		EOpenMobileHapticsPlaybackControlTransitionOutcome::InvalidState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackRemainingDurationTest,
	"OpenMobile.Haptics.Playback.Controls.RemainingDuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackRemainingDurationTest::RunTest(const FString& Parameters)
{
	FOpenMobileHapticsRepeatPlan Plan;
	Plan.PatternDurationSeconds = 2.0;
	Plan.TotalDurationSeconds = 2.0;
	Plan.MaximumDurationSeconds = 10.0;
	FOpenMobileHapticsPlaybackControlPolicy Policy(Plan,
		EOpenMobileHapticPlaybackState::Started, 0.0);
	Policy.Pause(1.0);
	TestEqual(TEXT("Paused wall time consumes no remaining playback"),
		Policy.Snapshot(20.0).RemainingDurationSeconds, 1.0);
	Policy.Resume(20.0);
	Policy.Seek(0.0, 20.0, 0.0);
	TestEqual(TEXT("Backward seek extends the remaining timeline"),
		Policy.Snapshot(20.0).RemainingDurationSeconds, 2.0);
	TestEqual(TEXT("Resumed playback consumes the new duration"),
		Policy.Snapshot(21.0).RemainingDurationSeconds, 1.0);
	Plan.bLoop = true;
	Plan.RepeatCount = 2;
	Plan.RepeatStartTimeSeconds = 0.5;
	Plan.RepeatDurationSeconds = 1.5;
	FOpenMobileHapticsPlaybackControlPolicy Repeating(Plan,
		EOpenMobileHapticPlaybackState::Started, 0.0);
	TestEqual(TEXT("Remaining duration includes pending repeats"),
		Repeating.Snapshot(2.5).RemainingDurationSeconds, 2.5);
	Plan.bRepeatUntilStopped = true;
	FOpenMobileHapticsPlaybackControlPolicy BoundedLoop(Plan,
		EOpenMobileHapticPlaybackState::Started, 0.0);
	TestEqual(TEXT("Indefinite repeats retain the active-duration cap"),
		BoundedLoop.Snapshot(7.0).RemainingDurationSeconds, 3.0);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "MovieSceneOpenMobileHapticsSection.h"
#include "MovieSceneOpenMobileHapticsTrack.h"
#include "OpenMobileHapticsSequencerPolicy.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsSequencerContractTest,
	"OpenMobile.Haptics.Integration.Sequencer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSequencerContractTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const auto Resolve = [](
		EMovieScenePlayerStatus::Type Status,
		EPlayDirection Direction,
		bool bSilent,
		bool bPreview,
		bool bHasHandle,
		bool bJumped,
		bool bLooped
	)
	{
		return FOpenMobileHapticsSequencerPolicy::Resolve(
			Status,
			Direction,
			bSilent,
			bPreview,
			bHasHandle,
			bJumped,
			bLooped
		);
	};

	TestEqual(TEXT("Section entry starts one request"),
		Resolve(EMovieScenePlayerStatus::Playing, EPlayDirection::Forwards,
			false, false, false, false, false).Action,
		EOpenMobileHapticsSequencerAction::Start);
	TestEqual(TEXT("Continuous playback does not duplicate requests"),
		Resolve(EMovieScenePlayerStatus::Playing, EPlayDirection::Forwards,
			false, false, true, false, false).Action,
		EOpenMobileHapticsSequencerAction::None);
	TestEqual(TEXT("Forward seek controls the current handle"),
		Resolve(EMovieScenePlayerStatus::Playing, EPlayDirection::Forwards,
			false, false, true, true, false).Action,
		EOpenMobileHapticsSequencerAction::Seek);
	TestEqual(TEXT("Loop restart replaces the current handle once"),
		Resolve(EMovieScenePlayerStatus::Playing, EPlayDirection::Forwards,
			false, false, true, true, true).Action,
		EOpenMobileHapticsSequencerAction::Restart);
	TestEqual(TEXT("Reverse playback stops active Haptics"),
		Resolve(EMovieScenePlayerStatus::Playing, EPlayDirection::Backwards,
			false, false, true, false, false).Action,
		EOpenMobileHapticsSequencerAction::Stop);
	TestEqual(TEXT("Transport stop clears active Haptics"),
		Resolve(EMovieScenePlayerStatus::Stopped, EPlayDirection::Forwards,
			false, false, true, false, false).Action,
		EOpenMobileHapticsSequencerAction::Stop);
	TestEqual(TEXT("Scrubbing is silent by default"),
		Resolve(EMovieScenePlayerStatus::Scrubbing, EPlayDirection::Forwards,
			false, false, true, true, false).Action,
		EOpenMobileHapticsSequencerAction::Stop);
	const FOpenMobileHapticsSequencerDecision PreviewStart = Resolve(
		EMovieScenePlayerStatus::Scrubbing,
		EPlayDirection::Forwards,
		false,
		true,
		false,
		true,
		false
	);
	TestEqual(TEXT("Explicit scrub preview starts once"), PreviewStart.Action,
		EOpenMobileHapticsSequencerAction::Start);
	TestTrue(TEXT("Explicit scrub preview seeks to transport time"),
		PreviewStart.bSeekAfterStart);
	TestEqual(TEXT("Silent evaluation never mutates a handle"),
		Resolve(EMovieScenePlayerStatus::Stopped, EPlayDirection::Forwards,
			true, false, true, false, false).Action,
		EOpenMobileHapticsSequencerAction::None);

	UMovieSceneOpenMobileHapticsTrack* Track =
		NewObject<UMovieSceneOpenMobileHapticsTrack>();
	TestFalse(TEXT("Skipped ranges never evaluate a nearest section"),
		Track->EvalOptions.bCanEvaluateNearestSection
			|| Track->EvalOptions.bEvalNearestSection);
	UMovieSceneSection* Section = Track->AddNewHapticsSection(20, 30);
	TestTrue(TEXT("Track creates the Haptics section type"),
		Section->IsA<UMovieSceneOpenMobileHapticsSection>());
	TestEqual(TEXT("Section starts at the requested boundary"),
		Section->GetInclusiveStartFrame(), FFrameNumber(20));
	TestEqual(TEXT("Section ends at the requested boundary"),
		Section->GetExclusiveEndFrame(), FFrameNumber(50));
	TestTrue(TEXT("Track owns its created section"), Track->HasSection(*Section));
	return true;
}

#endif

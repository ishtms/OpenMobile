#include "OpenMobileHapticsTrackEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISequencerSection.h"
#include "ISequencer.h"
#include "MovieScene.h"
#include "MovieSceneOpenMobileHapticsSection.h"
#include "MovieSceneOpenMobileHapticsTrack.h"
#include "ScopedTransaction.h"
#include "SequencerUtilities.h"

#define LOCTEXT_NAMESPACE "OpenMobileHapticsTrackEditor"

FOpenMobileHapticsTrackEditor::FOpenMobileHapticsTrackEditor(
	TSharedRef<ISequencer> Sequencer
)
	: FMovieSceneTrackEditor(Sequencer)
{
}

TSharedRef<ISequencerTrackEditor>
FOpenMobileHapticsTrackEditor::CreateTrackEditor(
	TSharedRef<ISequencer> Sequencer
)
{
	return MakeShared<FOpenMobileHapticsTrackEditor>(Sequencer);
}

void FOpenMobileHapticsTrackEditor::BuildAddTrackMenu(
	FMenuBuilder& MenuBuilder
)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddTrack", "OpenMobile Haptics Track"),
		LOCTEXT("AddTrackTooltip", "Adds a section-driven mobile Haptics track."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(
			this,
			&FOpenMobileHapticsTrackEditor::HandleAddTrack
		))
	);
}

TSharedPtr<SWidget>
FOpenMobileHapticsTrackEditor::BuildOutlinerEditWidget(
	const FGuid& ObjectBinding,
	UMovieSceneTrack* Track,
	const FBuildEditWidgetParams& Params
)
{
	static_cast<void>(ObjectBinding);
	UMovieSceneOpenMobileHapticsTrack* HapticsTrack =
		Cast<UMovieSceneOpenMobileHapticsTrack>(Track);
	if (!HapticsTrack)
	{
		return nullptr;
	}
	return FSequencerUtilities::MakeAddButton(
		LOCTEXT("AddSectionButton", "Haptics"),
		FOnGetContent::CreateRaw(
			this,
			&FOpenMobileHapticsTrackEditor::BuildSectionMenu,
			HapticsTrack
		),
		Params.NodeIsHovered,
		GetSequencer()
	);
}

TSharedRef<ISequencerSection>
FOpenMobileHapticsTrackEditor::MakeSectionInterface(
	UMovieSceneSection& Section,
	UMovieSceneTrack& Track,
	FGuid ObjectBinding
)
{
	static_cast<void>(Track);
	static_cast<void>(ObjectBinding);
	return MakeShared<FSequencerSection>(Section);
}

bool FOpenMobileHapticsTrackEditor::SupportsType(
	TSubclassOf<UMovieSceneTrack> TrackClass
) const
{
	return TrackClass == UMovieSceneOpenMobileHapticsTrack::StaticClass();
}

FKeyPropertyResult FOpenMobileHapticsTrackEditor::AddSection(
	FFrameNumber Time,
	UMovieSceneOpenMobileHapticsTrack* Track
)
{
	FKeyPropertyResult Result;
	if (!Track)
	{
		return Result;
	}
	Track->Modify();
	UMovieScene* MovieScene = Track->GetTypedOuter<UMovieScene>();
	const FFrameNumber Duration = MovieScene
		? MovieScene->GetTickResolution().AsFrameNumber(1.0)
		: FFrameNumber(1);
	UMovieSceneSection* Section = Track->AddNewHapticsSection(Time, Duration);
	Result.bTrackModified = true;
	Result.SectionsCreated.Add(Section);
	GetSequencer()->EmptySelection();
	GetSequencer()->SelectSection(Section);
	GetSequencer()->ThrobSectionSelection();
	return Result;
}

TSharedRef<SWidget> FOpenMobileHapticsTrackEditor::BuildSectionMenu(
	UMovieSceneOpenMobileHapticsTrack* Track
)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddSection", "Add Haptics Section"),
		LOCTEXT("AddSectionTooltip", "Adds a Haptics section at the current time."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Track]()
		{
			AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(
				this,
				&FOpenMobileHapticsTrackEditor::AddSection,
				Track
			));
		}))
	);
	return MenuBuilder.MakeWidget();
}

void FOpenMobileHapticsTrackEditor::HandleAddTrack()
{
	TSharedPtr<ISequencer> Sequencer = GetSequencer();
	UMovieScene* MovieScene = GetFocusedMovieScene();
	if (!Sequencer.IsValid() || !MovieScene || MovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(
		LOCTEXT("AddTrackTransaction", "Add OpenMobile Haptics Track")
	);
	MovieScene->Modify();
	UMovieSceneOpenMobileHapticsTrack* Track =
		MovieScene->AddTrack<UMovieSceneOpenMobileHapticsTrack>();
	if (!Track)
	{
		return;
	}

	const FFrameNumber Duration = MovieScene->GetTickResolution().AsFrameNumber(
		1.0
	);
	UMovieSceneSection* Section = Track->AddNewHapticsSection(
		GetTimeForKey(),
		Duration
	);
	Sequencer->OnAddTrack(Track, FGuid());
	Sequencer->EmptySelection();
	Sequencer->SelectSection(Section);
	Sequencer->ThrobSectionSelection();
}

#undef LOCTEXT_NAMESPACE

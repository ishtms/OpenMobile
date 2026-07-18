#pragma once

#include "MovieSceneTrackEditor.h"

class UMovieSceneOpenMobileHapticsTrack;

class FOpenMobileHapticsTrackEditor final : public FMovieSceneTrackEditor
{
public:
	explicit FOpenMobileHapticsTrackEditor(TSharedRef<ISequencer> Sequencer);

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(
		TSharedRef<ISequencer> Sequencer
	);

	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(
		const FGuid& ObjectBinding,
		UMovieSceneTrack* Track,
		const FBuildEditWidgetParams& Params
	) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(
		UMovieSceneSection& Section,
		UMovieSceneTrack& Track,
		FGuid ObjectBinding
	) override;
	virtual bool SupportsType(
		TSubclassOf<UMovieSceneTrack> TrackClass
	) const override;

private:
	FKeyPropertyResult AddSection(
		FFrameNumber Time,
		UMovieSceneOpenMobileHapticsTrack* Track
	);
	TSharedRef<SWidget> BuildSectionMenu(
		UMovieSceneOpenMobileHapticsTrack* Track
	);
	void HandleAddTrack();
};

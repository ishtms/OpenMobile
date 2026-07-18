#pragma once

#include "Compilation/IMovieSceneTrackTemplateProducer.h"
#include "CoreMinimal.h"
#include "MovieSceneNameableTrack.h"

#include "MovieSceneOpenMobileHapticsTrack.generated.h"

UCLASS(BlueprintType)
class OPENMOBILEHAPTICSSEQUENCER_API
	UMovieSceneOpenMobileHapticsTrack final
	: public UMovieSceneNameableTrack
	, public IMovieSceneTrackTemplateProducer
{
	GENERATED_BODY()

public:
	UMovieSceneOpenMobileHapticsTrack();

	UMovieSceneSection* AddNewHapticsSection(
		FFrameNumber StartTime,
		FFrameNumber Duration
	);

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(
		const UMovieSceneSection& Section
	) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual bool IsEmpty() const override;
	virtual void RemoveAllAnimationData() override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual bool SupportsMultipleRows() const override;
	virtual bool SupportsType(
		TSubclassOf<UMovieSceneSection> SectionClass
	) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

private:
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> Sections;
};

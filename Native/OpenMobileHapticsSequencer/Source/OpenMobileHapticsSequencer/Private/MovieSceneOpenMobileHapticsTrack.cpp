#include "MovieSceneOpenMobileHapticsTrack.h"

#include "MovieSceneOpenMobileHapticsSection.h"
#include "MovieSceneOpenMobileHapticsTemplate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneOpenMobileHapticsTrack)

#define LOCTEXT_NAMESPACE "MovieSceneOpenMobileHapticsTrack"

UMovieSceneOpenMobileHapticsTrack::UMovieSceneOpenMobileHapticsTrack()
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(230, 118, 40, 65);
#endif
	EvalOptions.bCanEvaluateNearestSection = false;
	EvalOptions.bEvaluateInPreroll = false;
	EvalOptions.bEvaluateInPostroll = false;
}

UMovieSceneSection*
UMovieSceneOpenMobileHapticsTrack::AddNewHapticsSection(
	FFrameNumber StartTime,
	FFrameNumber Duration
)
{
	UMovieSceneSection* Section = CreateNewSection();
	Section->InitialPlacement(
		Sections,
		StartTime,
		FMath::Max(1, Duration.Value),
		true
	);
	AddSection(*Section);
	return Section;
}

void UMovieSceneOpenMobileHapticsTrack::AddSection(
	UMovieSceneSection& Section
)
{
	Sections.Add(&Section);
}

FMovieSceneEvalTemplatePtr
UMovieSceneOpenMobileHapticsTrack::CreateTemplateForSection(
	const UMovieSceneSection& Section
) const
{
	return FMovieSceneOpenMobileHapticsSectionTemplate(
		*CastChecked<UMovieSceneOpenMobileHapticsSection>(&Section)
	);
}

UMovieSceneSection* UMovieSceneOpenMobileHapticsTrack::CreateNewSection()
{
	return NewObject<UMovieSceneOpenMobileHapticsSection>(
		this,
		NAME_None,
		RF_Transactional
	);
}

const TArray<UMovieSceneSection*>&
UMovieSceneOpenMobileHapticsTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneOpenMobileHapticsTrack::HasSection(
	const UMovieSceneSection& Section
) const
{
	return Sections.Contains(&Section);
}

bool UMovieSceneOpenMobileHapticsTrack::IsEmpty() const
{
	return Sections.IsEmpty();
}

void UMovieSceneOpenMobileHapticsTrack::RemoveAllAnimationData()
{
	Sections.Reset();
}

void UMovieSceneOpenMobileHapticsTrack::RemoveSection(
	UMovieSceneSection& Section
)
{
	Sections.Remove(&Section);
}

void UMovieSceneOpenMobileHapticsTrack::RemoveSectionAt(int32 SectionIndex)
{
	Sections.RemoveAt(SectionIndex);
}

bool UMovieSceneOpenMobileHapticsTrack::SupportsMultipleRows() const
{
	return true;
}

bool UMovieSceneOpenMobileHapticsTrack::SupportsType(
	TSubclassOf<UMovieSceneSection> SectionClass
) const
{
	return SectionClass == UMovieSceneOpenMobileHapticsSection::StaticClass();
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneOpenMobileHapticsTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "OpenMobile Haptics");
}
#endif

#undef LOCTEXT_NAMESPACE

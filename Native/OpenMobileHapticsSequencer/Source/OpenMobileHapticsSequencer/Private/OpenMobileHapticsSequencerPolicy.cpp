#include "OpenMobileHapticsSequencerPolicy.h"

FOpenMobileHapticsSequencerDecision
FOpenMobileHapticsSequencerPolicy::Resolve(
	EMovieScenePlayerStatus::Type Status,
	EPlayDirection Direction,
	bool bSilent,
	bool bPreviewWhileScrubbing,
	bool bHasHandle,
	bool bHasJumped,
	bool bHasLooped
)
{
	if (bSilent)
	{
		return {};
	}

	const bool bPlaying = Status == EMovieScenePlayerStatus::Playing
		&& Direction == EPlayDirection::Forwards;
	if (bPlaying)
	{
		if (bHasLooped)
		{
			return {EOpenMobileHapticsSequencerAction::Restart, false};
		}
		if (!bHasHandle)
		{
			return {
				EOpenMobileHapticsSequencerAction::Start,
				bHasJumped
			};
		}
		if (bHasJumped)
		{
			return {EOpenMobileHapticsSequencerAction::Seek, false};
		}
		return {};
	}

	const bool bScrubPreview = bPreviewWhileScrubbing
		&& (Status == EMovieScenePlayerStatus::Scrubbing
			|| Status == EMovieScenePlayerStatus::Jumping
			|| Status == EMovieScenePlayerStatus::Stepping);
	if (bScrubPreview)
	{
		return bHasHandle
			? FOpenMobileHapticsSequencerDecision{
				EOpenMobileHapticsSequencerAction::Seek,
				false
			}
			: FOpenMobileHapticsSequencerDecision{
				EOpenMobileHapticsSequencerAction::Start,
				true
			};
	}

	return bHasHandle
		? FOpenMobileHapticsSequencerDecision{
			EOpenMobileHapticsSequencerAction::Stop,
			false
		}
		: FOpenMobileHapticsSequencerDecision{};
}

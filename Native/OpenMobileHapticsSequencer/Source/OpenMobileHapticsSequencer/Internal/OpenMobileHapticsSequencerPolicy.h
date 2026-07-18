#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieScenePlayback.h"

enum class EOpenMobileHapticsSequencerAction : uint8
{
	None,
	Start,
	Stop,
	Seek,
	Restart
};

struct FOpenMobileHapticsSequencerDecision
{
	EOpenMobileHapticsSequencerAction Action =
		EOpenMobileHapticsSequencerAction::None;
	bool bSeekAfterStart = false;
};

struct FOpenMobileHapticsSequencerPolicy
{
	static FOpenMobileHapticsSequencerDecision Resolve(
		EMovieScenePlayerStatus::Type Status,
		EPlayDirection Direction,
		bool bSilent,
		bool bPreviewWhileScrubbing,
		bool bHasHandle,
		bool bHasJumped,
		bool bHasLooped
	);
};

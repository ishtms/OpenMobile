#include "MovieSceneOpenMobileHapticsTemplate.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Evaluation/MovieSceneExecutionTokens.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "IMovieScenePlayer.h"
#include "OpenMobileHapticsSubsystem.h"
#include "OpenMobileHapticsSequencerPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneOpenMobileHapticsTemplate)

namespace
{
struct FOpenMobileHapticsSectionData : IPersistentEvaluationData
{
	TWeakObjectPtr<UOpenMobileHapticsSubsystem> Subsystem;
	FOpenMobileHapticPlaybackHandle Handle;
};

void StopSection(FOpenMobileHapticsSectionData& Data)
{
	if (Data.Handle.IsValid())
	{
		if (UOpenMobileHapticsSubsystem* Subsystem = Data.Subsystem.Get())
		{
			Subsystem->StopPlaybackNative(Data.Handle);
		}
	}
	Data.Handle = FOpenMobileHapticPlaybackHandle();
	Data.Subsystem.Reset();
}

UOpenMobileHapticsSubsystem* ResolveSubsystem(IMovieScenePlayer& Player)
{
	UObject* PlaybackContext = Player.GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance
		? GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>()
		: nullptr;
}

struct FOpenMobileHapticsExecutionToken final : IMovieSceneExecutionToken
{
	explicit FOpenMobileHapticsExecutionToken(
		const FMovieSceneOpenMobileHapticsSectionTemplate& InTemplate
	)
		: Template(InTemplate)
	{
	}

	virtual void Execute(
		const FMovieSceneContext& Context,
		const FMovieSceneEvaluationOperand& Operand,
		FPersistentEvaluationData& PersistentData,
		IMovieScenePlayer& Player
	) override
	{
		static_cast<void>(Operand);
		FOpenMobileHapticsSectionData& Data =
			PersistentData.GetOrAddSectionData<
				FOpenMobileHapticsSectionData
			>();
		const EMovieScenePlayerStatus::Type Status = Context.GetStatus();
		const bool bPlaying = Status == EMovieScenePlayerStatus::Playing
			&& Context.GetDirection() == EPlayDirection::Forwards;
		const FOpenMobileHapticsSequencerDecision Decision =
			FOpenMobileHapticsSequencerPolicy::Resolve(
				Status,
				Context.GetDirection(),
				Context.IsSilent(),
				Template.bPreviewWhileScrubbing,
				Data.Handle.IsValid(),
				Context.HasJumped(),
				Context.HasLooped()
			);
		if (Decision.Action == EOpenMobileHapticsSequencerAction::None)
		{
			return;
		}
		if (Decision.Action == EOpenMobileHapticsSequencerAction::Stop)
		{
			StopSection(Data);
			return;
		}

		UOpenMobileHapticsSubsystem* Subsystem = ResolveSubsystem(Player);
		if (!Subsystem)
		{
			StopSection(Data);
			return;
		}

		if (Decision.Action == EOpenMobileHapticsSequencerAction::Restart)
		{
			StopSection(Data);
		}
		if (!Data.Handle.IsValid())
		{
			const FOpenMobileHapticPlaybackResult Result = Submit(*Subsystem);
			if (!Result.IsAccepted() || !Result.Handle.IsValid())
			{
				return;
			}
			Data.Subsystem = Subsystem;
			Data.Handle = Result.Handle;
		}

		if (Decision.Action == EOpenMobileHapticsSequencerAction::Seek
			|| Decision.bSeekAfterStart)
		{
			const double PositionSeconds = FMath::Max(
				0.0,
				Context.GetFrameRate().AsSeconds(
					Context.GetTime() - Template.SectionStartFrame
				)
			);
			const FOpenMobileHapticControlResult SeekResult =
				Subsystem->SeekPlaybackNative(Data.Handle, PositionSeconds);
			if (bPlaying
				&& SeekResult.Outcome
					!= EOpenMobileHapticControlOutcome::Accepted)
			{
				StopSection(Data);
				const FOpenMobileHapticPlaybackResult Restarted =
					Submit(*Subsystem);
				if (Restarted.IsAccepted() && Restarted.Handle.IsValid())
				{
					Data.Subsystem = Subsystem;
					Data.Handle = Restarted.Handle;
				}
			}
		}
	}

	FOpenMobileHapticPlaybackResult Submit(
		UOpenMobileHapticsSubsystem& Subsystem
	) const
	{
		const bool bValidSemantic = Template.EffectMode
			== EOpenMobileHapticsSequencerEffectMode::Semantic
			&& static_cast<uint8>(Template.SemanticEffect)
				<= static_cast<uint8>(
					EOpenMobileHapticSemanticEffect::Achievement
				);
		const bool bValidNamed = Template.EffectMode
			== EOpenMobileHapticsSequencerEffectMode::NamedPattern
			&& !Template.NamedPattern.IsNone();
		if ((!bValidSemantic && !bValidNamed)
			|| Template.Channel.IsNone()
			|| !FMath::IsFinite(Template.Intensity)
			|| Template.Intensity < 0.0f
			|| Template.Intensity > 1.0f
			|| !FMath::IsFinite(Template.IntensityScale)
			|| Template.IntensityScale < 0.0f
			|| Template.IntensityScale > 1.0f
			|| static_cast<uint8>(Template.Priority)
				> static_cast<uint8>(
					EOpenMobileHapticChannelPriority::Critical
				))
		{
			return FOpenMobileHapticPlaybackResult::MakeRejected(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("Invalid Sequencer Haptics section.")
			);
		}

		FOpenMobileHapticPlaybackOptions Options;
		Options.Category = Template.Category;
		Options.Channel = Template.Channel;
		Options.IntensityScale = Template.IntensityScale;
		Options.Priority = Template.Priority;

		if (bValidSemantic)
		{
			FOpenMobileHapticSemanticRequest Request;
			Request.Effect = Template.SemanticEffect;
			Request.Intensity = Template.Intensity;
			Request.Options = Options;
			return Subsystem.SubmitSemantic(Request);
		}

		FOpenMobileHapticNamedPatternRequest Request;
		Request.PatternName = Template.NamedPattern;
		Request.Intensity = Template.Intensity;
		Request.Options = Options;
		return Subsystem.SubmitNamedPattern(Request);
	}

	FMovieSceneOpenMobileHapticsSectionTemplate Template;
};
}

FMovieSceneOpenMobileHapticsSectionTemplate::
	FMovieSceneOpenMobileHapticsSectionTemplate(
		const UMovieSceneOpenMobileHapticsSection& Section
	)
	: EffectMode(Section.EffectMode)
	, SemanticEffect(Section.SemanticEffect)
	, NamedPattern(Section.NamedPattern)
	, Intensity(Section.Intensity)
	, Category(Section.Category)
	, IntensityScale(Section.IntensityScale)
	, Channel(Section.Channel)
	, Priority(Section.Priority)
	, bPreviewWhileScrubbing(Section.bPreviewWhileScrubbing)
	, SectionStartFrame(Section.GetInclusiveStartFrame())
{
}

void FMovieSceneOpenMobileHapticsSectionTemplate::Evaluate(
	const FMovieSceneEvaluationOperand& Operand,
	const FMovieSceneContext& Context,
	const FPersistentEvaluationData& PersistentData,
	FMovieSceneExecutionTokens& ExecutionTokens
) const
{
	static_cast<void>(Operand);
	static_cast<void>(Context);
	static_cast<void>(PersistentData);
	ExecutionTokens.Add(FOpenMobileHapticsExecutionToken(*this));
}

UScriptStruct&
FMovieSceneOpenMobileHapticsSectionTemplate::GetScriptStructImpl() const
{
	return *StaticStruct();
}

void FMovieSceneOpenMobileHapticsSectionTemplate::Initialize(
	const FMovieSceneEvaluationOperand& Operand,
	const FMovieSceneContext& Context,
	FPersistentEvaluationData& PersistentData,
	IMovieScenePlayer& Player
) const
{
	static_cast<void>(Operand);
	static_cast<void>(Context);
	static_cast<void>(Player);
	PersistentData.GetOrAddSectionData<FOpenMobileHapticsSectionData>();
}

void FMovieSceneOpenMobileHapticsSectionTemplate::SetupOverrides()
{
	EnableOverrides(RequiresInitializeFlag | RequiresTearDownFlag);
}

void FMovieSceneOpenMobileHapticsSectionTemplate::TearDown(
	FPersistentEvaluationData& PersistentData,
	IMovieScenePlayer& Player
) const
{
	static_cast<void>(Player);
	if (FOpenMobileHapticsSectionData* Data =
		PersistentData.FindSectionData<FOpenMobileHapticsSectionData>())
	{
		StopSection(*Data);
	}
}

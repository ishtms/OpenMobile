#pragma once

#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieSceneOpenMobileHapticsSection.h"

#include "MovieSceneOpenMobileHapticsTemplate.generated.h"

USTRUCT()
struct FMovieSceneOpenMobileHapticsSectionTemplate
	: public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneOpenMobileHapticsSectionTemplate() = default;
	explicit FMovieSceneOpenMobileHapticsSectionTemplate(
		const UMovieSceneOpenMobileHapticsSection& Section
	);

	virtual void Evaluate(
		const FMovieSceneEvaluationOperand& Operand,
		const FMovieSceneContext& Context,
		const FPersistentEvaluationData& PersistentData,
		FMovieSceneExecutionTokens& ExecutionTokens
	) const override;
	virtual UScriptStruct& GetScriptStructImpl() const override;
	virtual void Initialize(
		const FMovieSceneEvaluationOperand& Operand,
		const FMovieSceneContext& Context,
		FPersistentEvaluationData& PersistentData,
		IMovieScenePlayer& Player
	) const override;
	virtual void SetupOverrides() override;
	virtual void TearDown(
		FPersistentEvaluationData& PersistentData,
		IMovieScenePlayer& Player
	) const override;

	UPROPERTY()
	EOpenMobileHapticsSequencerEffectMode EffectMode =
		EOpenMobileHapticsSequencerEffectMode::Semantic;

	UPROPERTY()
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::ImpactMedium;

	UPROPERTY()
	FName NamedPattern;

	UPROPERTY()
	float Intensity = 1.0f;

	UPROPERTY()
	FName Category;

	UPROPERTY()
	float IntensityScale = 1.0f;

	UPROPERTY()
	FName Channel;

	UPROPERTY()
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY()
	bool bPreviewWhileScrubbing = false;

	UPROPERTY()
	FFrameNumber SectionStartFrame;
};

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "OpenMobileHapticsTypes.h"

#include "MovieSceneOpenMobileHapticsSection.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileHapticsSequencerEffectMode : uint8
{
	Semantic,
	NamedPattern
};

UCLASS(BlueprintType)
class OPENMOBILEHAPTICSSEQUENCER_API
	UMovieSceneOpenMobileHapticsSection final : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneOpenMobileHapticsSection();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticsSequencerEffectMode EffectMode =
		EOpenMobileHapticsSequencerEffectMode::Semantic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (EditCondition = "EffectMode == EOpenMobileHapticsSequencerEffectMode::Semantic", EditConditionHides))
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::ImpactMedium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (EditCondition = "EffectMode == EOpenMobileHapticsSequencerEffectMode::NamedPattern", EditConditionHides))
	FName NamedPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FName Category = TEXT("Cinematic");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FName Channel = TEXT("Cinematic");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bPreviewWhileScrubbing = false;
};

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

#include "AnimNotify_OpenMobileHaptics.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileHapticAnimNotifyEffectMode : uint8
{
	Semantic,
	NamedPattern
};

UCLASS(
	Const,
	hideCategories = Object,
	collapseCategories,
	meta = (DisplayName = "OpenMobile Haptics")
)
class OPENMOBILEHAPTICS_API UAnimNotify_OpenMobileHaptics final
	: public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_OpenMobileHaptics();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticAnimNotifyEffectMode EffectMode =
		EOpenMobileHapticAnimNotifyEffectMode::Semantic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (EditCondition = "EffectMode == EOpenMobileHapticAnimNotifyEffectMode::Semantic", EditConditionHides))
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::ImpactMedium;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (EditCondition = "EffectMode == EOpenMobileHapticAnimNotifyEffectMode::NamedPattern", EditConditionHides))
	FName NamedPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel = TEXT("Gameplay");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bSuppressOnDedicatedServer = true;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	friend class FOpenMobileHapticsAnimNotifyTest;

	bool Dispatch(USkeletalMeshComponent* MeshComp) const;
};

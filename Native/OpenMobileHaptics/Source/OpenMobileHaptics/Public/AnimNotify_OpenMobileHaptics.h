#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

#include "AnimNotify_OpenMobileHaptics.generated.h"

class UOpenMobileHapticPatternAsset;

UENUM(BlueprintType, meta = (ToolTip = "Authored Haptic source used by this Animation Notify."))
enum class EOpenMobileHapticAnimNotifyEffectMode : uint8
{
	Semantic UMETA(DisplayName = "Semantic Effect", ToolTip = "Plays one portable semantic effect without prepared content."),
	NamedPattern UMETA(DisplayName = "Named Pattern (Legacy)", ToolTip = "Plays one raw configured alias that the owning gameplay system must prepare first."),
	GamePreset UMETA(DisplayName = "Game Preset", ToolTip = "Plays one stable game preset, including a prepared configured override when available."),
	PatternAsset UMETA(DisplayName = "Pattern Asset", ToolTip = "Plays the selected authored portable pattern asset directly.")
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify", meta = (ToolTip = "Selects the authored source used when the Animation Notify fires."))
	EOpenMobileHapticAnimNotifyEffectMode EffectMode =
		EOpenMobileHapticAnimNotifyEffectMode::Semantic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify", meta = (EditCondition = "EffectMode == EOpenMobileHapticAnimNotifyEffectMode::Semantic", EditConditionHides, ToolTip = "Portable semantic effect. Dedicated impact, notification, and game nodes may use different recommended channels outside this notify."))
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::ImpactMedium;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify", meta = (EditCondition = "EffectMode == EOpenMobileHapticAnimNotifyEffectMode::GamePreset", EditConditionHides, ToolTip = "Stable game preset. Prepared configured overrides are preferred before semantic fallback."))
	EOpenMobileHapticGamePreset GamePreset =
		EOpenMobileHapticGamePreset::Bump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify", meta = (EditCondition = "EffectMode == EOpenMobileHapticAnimNotifyEffectMode::PatternAsset", EditConditionHides, ToolTip = "Direct authored pattern asset. The owning animation or gameplay system must keep any required prepared content ready."))
	TObjectPtr<UOpenMobileHapticPatternAsset> PatternAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify|Advanced", meta = (EditCondition = "EffectMode == EOpenMobileHapticAnimNotifyEffectMode::NamedPattern", EditConditionHides, GetOptions = "OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.GetConfiguredHapticPatternNames", ToolTip = "Legacy configured alias selected from Project Settings libraries. The owning animation or gameplay system must prepare its library before the notify fires."))
	FName NamedPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized request intensity from zero through one."))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify|Advanced", meta = (ToolTip = "Optional player-policy category. Empty uses the effect, asset, or project default category."))
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify|Advanced", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Additional normalized request scale multiplied with player and project policy."))
	float IntensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify|Advanced", meta = (ToolTip = "Project Haptics channel. Empty names are invalid; Gameplay is the notify default."))
	FName Channel = TEXT("Gameplay");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify|Advanced", meta = (ToolTip = "Request priority combined with the configured channel priority."))
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Notify|Advanced", meta = (ToolTip = "Suppresses this local feedback on dedicated servers. Normal gameplay should leave this enabled."))
	bool bSuppressOnDedicatedServer = true;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context
	) const override;
#endif

private:
	friend class FOpenMobileHapticsAnimNotifyTest;

	bool Dispatch(USkeletalMeshComponent* MeshComp) const;
};

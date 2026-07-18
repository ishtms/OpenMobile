#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueNotifyTypes.h"
#include "OpenMobileHapticsTypes.h"

#include "OpenMobileHapticsGameplayCueNotify.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileHapticsGameplayCueEffectMode : uint8
{
	Semantic,
	NamedPattern
};

UCLASS(
	Abstract,
	Blueprintable,
	meta = (
		DisplayName = "OpenMobile Haptics Gameplay Cue",
		ShowWorldContextPin
	),
	hideCategories = (Replication)
)
class OPENMOBILEHAPTICSGAMEPLAYABILITIES_API
	UOpenMobileHapticsGameplayCueNotify : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticsGameplayCueEffectMode EffectMode =
		EOpenMobileHapticsGameplayCueEffectMode::Semantic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (EditCondition = "EffectMode == EOpenMobileHapticsGameplayCueEffectMode::Semantic", EditConditionHides))
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::ImpactMedium;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (EditCondition = "EffectMode == EOpenMobileHapticsGameplayCueEffectMode::NamedPattern", EditConditionHides))
	FName NamedPattern;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Category;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel = TEXT("Gameplay");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EGameplayCueNotify_LocallyControlledSource LocalOwnerSource =
		EGameplayCueNotify_LocallyControlledSource::TargetActor;

	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;

protected:
	virtual bool OnExecute_Implementation(
		AActor* Target,
		const FGameplayCueParameters& Parameters
	) const override;

private:
	friend class FOpenMobileHapticsGameplayCueContractTest;

	bool Submit(
		AActor* Target,
		const FGameplayCueParameters& Parameters
	) const;
};

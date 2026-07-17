#pragma once

#include "CoreMinimal.h"
#include "Extensions/UIComponent.h"
#include "OpenMobileHapticsTypes.h"
#include "Types/SlateEnums.h"

#include "OpenMobileHapticsWidgetComponent.generated.h"

class SWidget;
class UWidget;
struct FNavigationTransition;
struct FNavigationTransitionMetadata;

UENUM(BlueprintType)
enum class EOpenMobileHapticsUMGEffectMode : uint8
{
	Disabled,
	Semantic,
	NamedPattern
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSUMG_API FOpenMobileHapticsUMGEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticsUMGEffectMode Mode =
		EOpenMobileHapticsUMGEffectMode::Disabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (EditCondition = "Mode == EOpenMobileHapticsUMGEffectMode::Semantic", EditConditionHides))
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::Selection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (EditCondition = "Mode == EOpenMobileHapticsUMGEffectMode::NamedPattern", EditConditionHides))
	FName NamedPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (EditCondition = "Mode != EOpenMobileHapticsUMGEffectMode::Disabled"))
	FName Channel = TEXT("UI");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (EditCondition = "Mode != EOpenMobileHapticsUMGEffectMode::Disabled"))
	FName Category = TEXT("UI");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Mode != EOpenMobileHapticsUMGEffectMode::Disabled"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Mode != EOpenMobileHapticsUMGEffectMode::Disabled"))
	float IntensityScale = 1.0f;
};

UCLASS(BlueprintType, meta = (DisplayName = "OpenMobile Haptics"))
class OPENMOBILEHAPTICSUMG_API UOpenMobileHapticsWidgetComponent final
	: public UUIComponent
{
	GENERATED_BODY()

public:
	UOpenMobileHapticsWidgetComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticsUMGEffect ButtonPressed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticsUMGEffect SelectionChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticsUMGEffect Hovered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticsUMGEffect SliderStep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticsUMGEffect NavigationFocused;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float SliderStepSize = 0.1f;

	virtual TSharedRef<SWidget> RebuildWidgetWithContent(
		TSharedRef<SWidget> OwnerContent
	) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValidForOwner(
		const UWidget* OwnerWidget,
		FDataValidationContext& Context
	) const override;
#endif

protected:
	virtual void OnConstruct() override;
	virtual void OnDestruct() override;

private:
	friend class FOpenMobileHapticsUMGContractTest;
	friend class SOpenMobileHapticsHoverWidget;

	UFUNCTION()
	void HandleButtonPressed();

	UFUNCTION()
	void HandleComboSelectionChanged(
		FString SelectedItem,
		ESelectInfo::Type SelectionType
	);

	void HandleListSelectionChanged(UObject* Item);

	UFUNCTION()
	void HandleSliderValueChanged(float Value);

	void HandleHover();
	void HandleNavigationTransition(const FNavigationTransition& Transition);
	void Unbind();
	bool PlayEffect(const FOpenMobileHapticsUMGEffect& Effect) const;
	int32 ResolveSliderStep(float NormalizedValue) const;

	bool bHasSliderStep = false;
	int32 LastSliderStep = 0;
	TSharedPtr<FNavigationTransitionMetadata> NavigationMetadata;
	TWeakPtr<SWidget> NavigationWidget;
};

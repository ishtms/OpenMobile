#include "OpenMobileHapticsWidgetComponent.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/ListView.h"
#include "Components/Slider.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Input/NavigationMetadata.h"
#include "Misc/DataValidation.h"
#include "OpenMobileHapticsSubsystem.h"
#include "Widgets/SCompoundWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenMobileHapticsWidgetComponent)

class SOpenMobileHapticsHoverWidget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOpenMobileHapticsHoverWidget)
		: _Content()
	{
	}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(
		const FArguments& Arguments,
		UOpenMobileHapticsWidgetComponent* InComponent
	)
	{
		Component = InComponent;
		ChildSlot[Arguments._Content.Widget];
	}

	virtual void OnMouseEnter(
		const FGeometry& Geometry,
		const FPointerEvent& MouseEvent
	) override
	{
		const bool bWasHovered = IsHovered();
		SWidget::OnMouseEnter(Geometry, MouseEvent);
		if (!bWasHovered && IsHovered() && !MouseEvent.IsTouchEvent())
		{
			if (UOpenMobileHapticsWidgetComponent* Pinned = Component.Get())
			{
				Pinned->HandleHover();
			}
		}
	}

private:
	TWeakObjectPtr<UOpenMobileHapticsWidgetComponent> Component;
};

UOpenMobileHapticsWidgetComponent::UOpenMobileHapticsWidgetComponent()
{
	ButtonPressed.Mode = EOpenMobileHapticsUMGEffectMode::Semantic;
	ButtonPressed.SemanticEffect = EOpenMobileHapticSemanticEffect::Click;
	SelectionChanged.Mode = EOpenMobileHapticsUMGEffectMode::Semantic;
	SliderStep.Mode = EOpenMobileHapticsUMGEffectMode::Semantic;
	NavigationFocused.Mode = EOpenMobileHapticsUMGEffectMode::Semantic;
}

TSharedRef<SWidget> UOpenMobileHapticsWidgetComponent::RebuildWidgetWithContent(
	TSharedRef<SWidget> OwnerContent
)
{
	return SNew(SOpenMobileHapticsHoverWidget, this)[OwnerContent];
}

#if WITH_EDITOR
EDataValidationResult
UOpenMobileHapticsWidgetComponent::IsDataValidForOwner(
	const UWidget* OwnerWidget,
	FDataValidationContext& Context
) const
{
	static_cast<void>(OwnerWidget);
	bool bValid = FMath::IsFinite(SliderStepSize)
		&& SliderStepSize > 0.0f
		&& SliderStepSize <= 1.0f;
	if (!bValid)
	{
		Context.AddError(FText::FromString(
			TEXT("Slider step size must be finite and between 0 and 1.")
		));
	}
	const FOpenMobileHapticsUMGEffect* Effects[] = {
		&ButtonPressed,
		&SelectionChanged,
		&Hovered,
		&SliderStep,
		&NavigationFocused
	};
	for (const FOpenMobileHapticsUMGEffect* Effect : Effects)
	{
		if (Effect->Mode == EOpenMobileHapticsUMGEffectMode::Disabled)
		{
			continue;
		}
		const bool bEffectValid = !Effect->Channel.IsNone()
			&& static_cast<uint8>(Effect->Mode)
				<= static_cast<uint8>(
					EOpenMobileHapticsUMGEffectMode::NamedPattern
				)
			&& FMath::IsFinite(Effect->Intensity)
			&& Effect->Intensity >= 0.0f
			&& Effect->Intensity <= 1.0f
			&& FMath::IsFinite(Effect->IntensityScale)
			&& Effect->IntensityScale >= 0.0f
			&& Effect->IntensityScale <= 1.0f
			&& (Effect->Mode != EOpenMobileHapticsUMGEffectMode::NamedPattern
				|| !Effect->NamedPattern.IsNone())
			&& (Effect->Mode != EOpenMobileHapticsUMGEffectMode::Semantic
				|| static_cast<uint8>(Effect->SemanticEffect)
					<= static_cast<uint8>(
						EOpenMobileHapticSemanticEffect::Achievement
					));
		if (!bEffectValid)
		{
			bValid = false;
			Context.AddError(FText::FromString(
				TEXT("Enabled haptic bindings require a valid effect, channel, and normalized finite scales.")
			));
		}
	}
	return bValid
		? EDataValidationResult::Valid
		: EDataValidationResult::Invalid;
}
#endif

void UOpenMobileHapticsWidgetComponent::OnConstruct()
{
	Unbind();
	UWidget* Owner = GetOwner().Get();
	if (!Owner)
	{
		return;
	}

	if (UButton* Button = Cast<UButton>(Owner))
	{
		Button->OnPressed.AddUniqueDynamic(
			this,
			&UOpenMobileHapticsWidgetComponent::HandleButtonPressed
		);
	}
	if (UComboBoxString* ComboBox = Cast<UComboBoxString>(Owner))
	{
		ComboBox->OnSelectionChanged.AddUniqueDynamic(
			this,
			&UOpenMobileHapticsWidgetComponent::HandleComboSelectionChanged
		);
	}
	if (UListView* ListView = Cast<UListView>(Owner))
	{
		ListView->OnItemSelectionChanged().AddUObject(
			this,
			&UOpenMobileHapticsWidgetComponent::HandleListSelectionChanged
		);
	}
	if (USlider* Slider = Cast<USlider>(Owner))
	{
		LastSliderStep = ResolveSliderStep(Slider->GetNormalizedValue());
		bHasSliderStep = true;
		Slider->OnValueChanged.AddUniqueDynamic(
			this,
			&UOpenMobileHapticsWidgetComponent::HandleSliderValueChanged
		);
	}

	TSharedPtr<SWidget> CachedWidget = Owner->GetCachedWidget();
	if (CachedWidget.IsValid()
		&& NavigationFocused.Mode
			!= EOpenMobileHapticsUMGEffectMode::Disabled)
	{
		NavigationMetadata = MakeShared<FNavigationTransitionMetadata>();
		NavigationMetadata->OnNavigationTransition.BindUObject(
			this,
			&UOpenMobileHapticsWidgetComponent::HandleNavigationTransition
		);
		CachedWidget->AddMetadata(NavigationMetadata.ToSharedRef());
		NavigationWidget = CachedWidget;
	}
}

void UOpenMobileHapticsWidgetComponent::OnDestruct()
{
	Unbind();
}

void UOpenMobileHapticsWidgetComponent::Unbind()
{
	if (UWidget* Owner = GetOwner().Get())
	{
		if (UButton* Button = Cast<UButton>(Owner))
		{
			Button->OnPressed.RemoveDynamic(
				this,
				&UOpenMobileHapticsWidgetComponent::HandleButtonPressed
			);
		}
		if (UComboBoxString* ComboBox = Cast<UComboBoxString>(Owner))
		{
			ComboBox->OnSelectionChanged.RemoveDynamic(
				this,
				&UOpenMobileHapticsWidgetComponent::HandleComboSelectionChanged
			);
		}
		if (UListView* ListView = Cast<UListView>(Owner))
		{
			ListView->OnItemSelectionChanged().RemoveAll(this);
		}
		if (USlider* Slider = Cast<USlider>(Owner))
		{
			Slider->OnValueChanged.RemoveDynamic(
				this,
				&UOpenMobileHapticsWidgetComponent::HandleSliderValueChanged
			);
		}
	}

	if (NavigationMetadata.IsValid())
	{
		if (TSharedPtr<SWidget> Widget = NavigationWidget.Pin())
		{
			Widget->RemoveMetaData(NavigationMetadata.ToSharedRef());
		}
	}
	NavigationMetadata.Reset();
	NavigationWidget.Reset();
	bHasSliderStep = false;
}

void UOpenMobileHapticsWidgetComponent::HandleButtonPressed()
{
	PlayEffect(ButtonPressed);
}

void UOpenMobileHapticsWidgetComponent::HandleComboSelectionChanged(
	FString SelectedItem,
	ESelectInfo::Type SelectionType
)
{
	static_cast<void>(SelectedItem);
	if (SelectionType != ESelectInfo::Direct)
	{
		PlayEffect(SelectionChanged);
	}
}

void UOpenMobileHapticsWidgetComponent::HandleListSelectionChanged(
	UObject* Item
)
{
	static_cast<void>(Item);
	PlayEffect(SelectionChanged);
}

void UOpenMobileHapticsWidgetComponent::HandleSliderValueChanged(float Value)
{
	static_cast<void>(Value);
	const USlider* Slider = Cast<USlider>(GetOwner().Get());
	if (!Slider)
	{
		return;
	}
	const int32 Step = ResolveSliderStep(Slider->GetNormalizedValue());
	if (!bHasSliderStep)
	{
		LastSliderStep = Step;
		bHasSliderStep = true;
		return;
	}
	if (Step != LastSliderStep)
	{
		LastSliderStep = Step;
		PlayEffect(SliderStep);
	}
}

void UOpenMobileHapticsWidgetComponent::HandleHover()
{
	PlayEffect(Hovered);
}

void UOpenMobileHapticsWidgetComponent::HandleNavigationTransition(
	const FNavigationTransition& Transition
)
{
	if (Transition.Direction == ENavigationTransitionDirection::Incoming)
	{
		PlayEffect(NavigationFocused);
	}
}

bool UOpenMobileHapticsWidgetComponent::PlayEffect(
	const FOpenMobileHapticsUMGEffect& Effect
) const
{
	if (Effect.Mode == EOpenMobileHapticsUMGEffectMode::Disabled
		|| Effect.Channel.IsNone()
		|| !FMath::IsFinite(Effect.Intensity)
		|| Effect.Intensity < 0.0f
		|| Effect.Intensity > 1.0f
		|| !FMath::IsFinite(Effect.IntensityScale)
		|| Effect.IntensityScale < 0.0f
		|| Effect.IntensityScale > 1.0f)
	{
		return false;
	}
	const UWidget* Owner = GetOwner().Get();
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UOpenMobileHapticsSubsystem* Subsystem = GameInstance
		? GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>()
		: nullptr;
	if (!Subsystem)
	{
		return false;
	}

	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = Effect.Channel;
	Options.Category = Effect.Category;
	Options.IntensityScale = Effect.IntensityScale;
	if (Effect.Mode == EOpenMobileHapticsUMGEffectMode::Semantic)
	{
		FOpenMobileHapticSemanticRequest Request;
		Request.Effect = Effect.SemanticEffect;
		Request.Intensity = Effect.Intensity;
		Request.Options = Options;
		return Subsystem->SubmitSemantic(Request).IsAccepted();
	}
	if (Effect.Mode == EOpenMobileHapticsUMGEffectMode::NamedPattern
		&& !Effect.NamedPattern.IsNone())
	{
		FOpenMobileHapticNamedPatternRequest Request;
		Request.PatternName = Effect.NamedPattern;
		Request.Intensity = Effect.Intensity;
		Request.Options = Options;
		return Subsystem->SubmitNamedPattern(Request).IsAccepted();
	}
	return false;
}

int32 UOpenMobileHapticsWidgetComponent::ResolveSliderStep(
	float NormalizedValue
) const
{
	if (!FMath::IsFinite(NormalizedValue)
		|| !FMath::IsFinite(SliderStepSize)
		|| SliderStepSize <= 0.0f
		|| SliderStepSize > 1.0f)
	{
		return 0;
	}
	return FMath::FloorToInt(
		(FMath::Clamp(NormalizedValue, 0.0f, 1.0f) + UE_KINDA_SMALL_NUMBER)
		/ SliderStepSize
	);
}

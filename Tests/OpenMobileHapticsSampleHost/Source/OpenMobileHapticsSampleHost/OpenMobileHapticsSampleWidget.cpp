#include "OpenMobileHapticsSampleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileHapticsSampleRecipes.h"
#include "OpenMobileHapticsSubsystem.h"
#include "TimerManager.h"

namespace OpenMobileHapticsSampleWidgetPrivate
{
	const FLinearColor MutedColor(0.72f, 0.77f, 0.86f, 1.0f);
	const FLinearColor SuccessColor(0.38f, 0.93f, 0.65f, 1.0f);
	const FLinearColor WarningColor(1.0f, 0.72f, 0.30f, 1.0f);
	const FLinearColor ErrorColor(1.0f, 0.40f, 0.40f, 1.0f);

	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetDisplayNameTextByValue(static_cast<int64>(Value)).ToString()
			: TEXT("Unknown");
	}

	void SetFontSize(UTextBlock* TextBlock, int32 Size)
	{
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = Size;
		TextBlock->SetFont(Font);
	}

	void AddWithPadding(
		UVerticalBox* Parent,
		UWidget* Child,
		const FMargin& Padding
	)
	{
		UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}

	UTextBlock* AddText(
		UWidgetTree* WidgetTree,
		UVerticalBox* Parent,
		FName Name,
		const FString& Value,
		int32 Size,
		const FLinearColor& Color,
		const FMargin& Padding
	)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			Name
		);
		Text->SetText(FText::FromString(Value));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		SetFontSize(Text, Size);
		AddWithPadding(Parent, Text, Padding);
		return Text;
	}

	UButton* AddButton(
		UWidgetTree* WidgetTree,
		UUniformGridPanel* Grid,
		FName Name,
		const FString& Label,
		int32 Index
	)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			Name
		);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*(Name.ToString() + TEXT("Label")))
		);
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		SetFontSize(Text, 15);
		Button->SetContent(Text);
		UUniformGridSlot* Slot = Grid->AddChildToUniformGrid(
			Button,
			Index / 2,
			Index % 2
		);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
		return Button;
	}

	bool IsTerminal(EOpenMobileHapticPlaybackState State)
	{
		return State == EOpenMobileHapticPlaybackState::Stopped
			|| State == EOpenMobileHapticPlaybackState::Cancelled
			|| State == EOpenMobileHapticPlaybackState::Completed
			|| State == EOpenMobileHapticPlaybackState::Interrupted
			|| State == EOpenMobileHapticPlaybackState::Failed;
	}
}

TSharedRef<SWidget> UOpenMobileHapticsSampleWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UOpenMobileHapticsSampleWidget::BuildWidgetTree()
{
	using namespace OpenMobileHapticsSampleWidgetPrivate;
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("HapticsBackground")
	);
	Background->SetBrushColor(FLinearColor(0.018f, 0.024f, 0.035f, 1.0f));
	Background->SetPadding(FMargin(24.0f));
	WidgetTree->RootWidget = Background;

	UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("HapticsScrollBox")
	);
	Background->SetContent(ScrollBox);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("HapticsColumn")
	);
	ScrollBox->AddChild(Column);

	AddText(
		WidgetTree,
		Column,
		TEXT("Title"),
		TEXT("OpenMobile Haptics"),
		30,
		FLinearColor(0.62f, 0.48f, 1.0f, 1.0f),
		FMargin(0.0f, 0.0f, 0.0f, 4.0f)
	);
	AddText(
		WidgetTree,
		Column,
		TEXT("Subtitle"),
		TEXT("Portable effects, prepared patterns, handles, channels, scheduling, policy, and lifecycle events"),
		16,
		MutedColor,
		FMargin(0.0f, 0.0f, 0.0f, 10.0f)
	);
	AddText(
		WidgetTree,
		Column,
		TEXT("ComfortWarning"),
		TEXT("Keep feedback short and event-driven. Never use constant background vibration. Provide an in-game disable switch and respect accessibility preferences."),
		14,
		WarningColor,
		FMargin(0.0f, 0.0f, 0.0f, 14.0f)
	);

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass(),
		TEXT("HapticsControls")
	);
	Grid->SetMinDesiredSlotWidth(190.0f);
	Grid->SetMinDesiredSlotHeight(44.0f);
	const TArray<TPair<FName, FString>> ButtonDefinitions = {
		{TEXT("RefreshButton"), TEXT("Refresh status")},
		{TEXT("PrepareButton"), TEXT("Prepare starter patterns")},
		{TEXT("ReleaseButton"), TEXT("Release prepared patterns")},
		{TEXT("SelectionButton"), TEXT("UI selection")},
		{TEXT("UIConfirmButton"), TEXT("Prepared UI confirm")},
		{TEXT("CombatButton"), TEXT("Prepared combat impact")},
		{TEXT("DamageButton"), TEXT("Damage preset")},
		{TEXT("PickupButton"), TEXT("Pickup preset")},
		{TEXT("RewardButton"), TEXT("Prepared reward")},
		{TEXT("VehicleButton"), TEXT("Bounded vehicle texture")},
		{TEXT("ChargingButton"), TEXT("Charging connected")},
		{TEXT("AudioSyncButton"), TEXT("Audio-clock reward")},
		{TEXT("AccessibilityButton"), TEXT("Accessible confirmation")},
		{TEXT("ScheduleButton"), TEXT("Schedule reward in 1.5 s")},
		{TEXT("CancelButton"), TEXT("Cancel active handle")},
		{TEXT("StopGameplayButton"), TEXT("Stop Gameplay channel")},
		{TEXT("ToggleEnabledButton"), TEXT("Toggle global enabled")},
		{TEXT("SofterButton"), TEXT("Master intensity 35%")},
		{TEXT("FullIntensityButton"), TEXT("Master intensity 100%")}
	};
	Buttons.Reset(ButtonDefinitions.Num());
	for (int32 Index = 0; Index < ButtonDefinitions.Num(); ++Index)
	{
		Buttons.Add(AddButton(
			WidgetTree,
			Grid,
			ButtonDefinitions[Index].Key,
			ButtonDefinitions[Index].Value,
			Index
		));
	}
	AddWithPadding(Column, Grid, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	StatusText = AddText(
		WidgetTree,
		Column,
		TEXT("ActionStatus"),
		TEXT("Ready. Prepare the starter library for named examples."),
		15,
		MutedColor,
		FMargin(0.0f, 0.0f, 0.0f, 14.0f)
	);
	SummaryText = AddText(
		WidgetTree,
		Column,
		TEXT("HapticsSummary"),
		TEXT("Haptics status unavailable."),
		15,
		FLinearColor::White,
		FMargin(0.0f, 0.0f, 0.0f, 14.0f)
	);
	EventText = AddText(
		WidgetTree,
		Column,
		TEXT("PlaybackEvents"),
		TEXT("PLAYBACK LIFECYCLE\nNo events yet."),
		14,
		MutedColor,
		FMargin(0.0f, 0.0f, 0.0f, 14.0f)
	);
	AddText(
		WidgetTree,
		Column,
		TEXT("BlueprintRecipes"),
		TEXT(
			"BLUEPRINT RECIPES\n"
			"Get Haptics Subsystem, then Get Haptic Capabilities before choosing rich or semantic output.\n"
			"Preload Named Haptic Libraries and wait for On Named Haptic Libraries Prepared before named playback.\n"
			"Store accepted Playback Handle values, then Stop, Cancel, Pause, Resume, Seek, or Update Playback Parameters.\n"
			"Use Stop Haptic Channel for an owned gameplay layer and On Haptic Playback Event for lifecycle UI.\n"
			"Use Automatic fallback for normal gameplay. Use Exact Only only when silence is preferable to substitution."
		),
		14,
		MutedColor,
		FMargin(0.0f, 0.0f, 0.0f, 24.0f)
	);
}

void UOpenMobileHapticsSampleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Buttons.Num() != 19)
	{
		return;
	}
	Buttons[0]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleRefreshClicked);
	Buttons[1]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandlePrepareClicked);
	Buttons[2]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleReleaseClicked);
	Buttons[3]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleSelectionClicked);
	Buttons[4]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleUIConfirmClicked);
	Buttons[5]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleCombatClicked);
	Buttons[6]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleDamageClicked);
	Buttons[7]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandlePickupClicked);
	Buttons[8]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleRewardClicked);
	Buttons[9]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleVehicleClicked);
	Buttons[10]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleChargingClicked);
	Buttons[11]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleAudioSyncClicked);
	Buttons[12]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleAccessibilityClicked);
	Buttons[13]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleScheduleClicked);
	Buttons[14]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleCancelClicked);
	Buttons[15]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleStopGameplayClicked);
	Buttons[16]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleToggleEnabledClicked);
	Buttons[17]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleSofterClicked);
	Buttons[18]->OnClicked.AddUniqueDynamic(this, &UOpenMobileHapticsSampleWidget::HandleFullIntensityClicked);

	Haptics = UOpenMobileHapticsSampleRecipes::GetHapticsSubsystem(this);
	if (!Haptics)
	{
		SetStatus(
			TEXT("Haptics subsystem unavailable."),
			OpenMobileHapticsSampleWidgetPrivate::ErrorColor
		);
		return;
	}
	Haptics->OnPlaybackEvent.AddUniqueDynamic(
		this,
		&UOpenMobileHapticsSampleWidget::HandlePlaybackEvent
	);
	Haptics->OnNamedLibrariesPrepared.AddUniqueDynamic(
		this,
		&UOpenMobileHapticsSampleWidget::HandleLibrariesPrepared
	);
	RefreshSummary();
}

void UOpenMobileHapticsSampleWidget::NativeDestruct()
{
	StopVehicleTimer();
	if (Haptics)
	{
		Haptics->OnPlaybackEvent.RemoveDynamic(
			this,
			&UOpenMobileHapticsSampleWidget::HandlePlaybackEvent
		);
		Haptics->OnNamedLibrariesPrepared.RemoveDynamic(
			this,
			&UOpenMobileHapticsSampleWidget::HandleLibrariesPrepared
		);
		StopOwnedPlayback();
		if (PreloadHandle.IsValid())
		{
			Haptics->CancelNamedLibraryPreload(PreloadHandle);
		}
		Haptics->ReleaseNamedLibraries();
	}
	Haptics = nullptr;
	Super::NativeDestruct();
}

void UOpenMobileHapticsSampleWidget::RefreshSummary()
{
	using namespace OpenMobileHapticsSampleWidgetPrivate;
	if (!Haptics || !SummaryText)
	{
		return;
	}
	const FOpenMobileHapticCapabilities Capabilities =
		Haptics->GetHapticCapabilities();
	const FOpenMobileHapticsDiagnostics Diagnostics = Haptics->GetDiagnostics();
	SummaryText->SetText(FText::FromString(FString::Printf(
		TEXT(
			"CAPABILITIES AND POLICY\n"
			"Availability: %s | Basic: %s | Semantic: %s | Rich: %s\n"
			"Enabled: %s | Master intensity: %.0f%% | Preparation: %s\n"
			"Active: %d | Queued: %d | Prepared: %d | Fallbacks: %lld\n"
			"UI_Confirm: %s | Combat_Impact: %s | Reward_Success: %s | Vehicle_Bump: %s"
		),
		*EnumName(Capabilities.Availability),
		*EnumName(Capabilities.BasicVibration),
		*EnumName(Capabilities.SemanticFeedback),
		*EnumName(Capabilities.RichHaptics),
		Haptics->IsHapticsEnabled() ? TEXT("Yes") : TEXT("No"),
		Haptics->GetMasterIntensity() * 100.0f,
		*EnumName(Haptics->GetPreparationState()),
		Diagnostics.ActivePlaybackCount,
		Diagnostics.QueuedPlaybackCount,
		Diagnostics.PreparedNamedPatternCount,
		Diagnostics.FallbackPlaybackCount,
		*EnumName(Haptics->GetNamedPatternStatus(TEXT("UI_Confirm"))),
		*EnumName(Haptics->GetNamedPatternStatus(TEXT("Combat_Impact"))),
		*EnumName(Haptics->GetNamedPatternStatus(TEXT("Reward_Success"))),
		*EnumName(Haptics->GetNamedPatternStatus(TEXT("Vehicle_Bump")))
	)));
}

void UOpenMobileHapticsSampleWidget::ShowPlaybackResult(
	const FString& Action,
	const FOpenMobileHapticPlaybackResult& Result
)
{
	using namespace OpenMobileHapticsSampleWidgetPrivate;
	if (Result.IsAccepted() && Result.Handle.IsValid())
	{
		ActiveHandle = Result.Handle;
	}
	const FString Detail = Result.Error.IsSet()
		? Result.Error.Message
		: Result.ResolvedPath.ToString();
	SetStatus(
		FString::Printf(
			TEXT("%s: %s | %s | %s"),
			*Action,
			*EnumName(Result.Outcome),
			*EnumName(Result.State),
			Detail.IsEmpty() ? TEXT("No detail") : *Detail
		),
		Result.IsAccepted() ? SuccessColor : ErrorColor
	);
	RefreshSummary();
}

void UOpenMobileHapticsSampleWidget::ShowControlResult(
	const FString& Action,
	const FOpenMobileHapticControlResult& Result
)
{
	using namespace OpenMobileHapticsSampleWidgetPrivate;
	SetStatus(
		FString::Printf(
			TEXT("%s: %s | %s"),
			*Action,
			*EnumName(Result.Outcome),
			Result.Error.IsSet()
				? *Result.Error.Message
				: *EnumName(Result.State)
		),
		Result.Outcome == EOpenMobileHapticControlOutcome::Accepted
			? SuccessColor
			: WarningColor
	);
	RefreshSummary();
}

void UOpenMobileHapticsSampleWidget::SetStatus(
	const FString& Message,
	const FLinearColor& Color
)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UOpenMobileHapticsSampleWidget::StopOwnedPlayback()
{
	if (!Haptics)
	{
		return;
	}
	if (VehicleHandle.IsValid())
	{
		Haptics->StopPlayback(VehicleHandle);
	}
	if (ActiveHandle.IsValid() && ActiveHandle != VehicleHandle)
	{
		Haptics->StopPlayback(ActiveHandle);
	}
	VehicleHandle = {};
	ActiveHandle = {};
}

void UOpenMobileHapticsSampleWidget::StopVehicleTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VehicleUpdateTimer);
	}
}

void UOpenMobileHapticsSampleWidget::HandleRefreshClicked()
{
	RefreshSummary();
}

void UOpenMobileHapticsSampleWidget::HandlePrepareClicked()
{
	if (!Haptics)
	{
		return;
	}
	PreloadHandle = Haptics->PreloadNamedLibraries();
	SetStatus(
		PreloadHandle.IsValid()
			? TEXT("Preparing named Haptics libraries asynchronously.")
			: TEXT("Preparation did not start. Check the current preparation state."),
		PreloadHandle.IsValid()
			? OpenMobileHapticsSampleWidgetPrivate::MutedColor
			: OpenMobileHapticsSampleWidgetPrivate::WarningColor
	);
	RefreshSummary();
}

void UOpenMobileHapticsSampleWidget::HandleReleaseClicked()
{
	if (!Haptics)
	{
		return;
	}
	StopOwnedPlayback();
	StopVehicleTimer();
	Haptics->ReleaseNamedLibraries();
	PreloadHandle = {};
	SetStatus(
		TEXT("Released prepared named patterns."),
		OpenMobileHapticsSampleWidgetPrivate::MutedColor
	);
	RefreshSummary();
}

void UOpenMobileHapticsSampleWidget::HandleSelectionClicked()
{
	if (Haptics)
	{
		ShowPlaybackResult(
			TEXT("UI selection"),
			Haptics->PlaySelectionFeedback(0.25f, TEXT("UI"))
		);
	}
}

void UOpenMobileHapticsSampleWidget::HandleUIConfirmClicked()
{
	ShowPlaybackResult(
		TEXT("Prepared UI confirm"),
		UOpenMobileHapticsSampleRecipes::PlayPreparedPattern(
			this,
			TEXT("UI_Confirm"),
			0.4f,
			TEXT("UI")
		)
	);
}

void UOpenMobileHapticsSampleWidget::HandleCombatClicked()
{
	ShowPlaybackResult(
		TEXT("Prepared combat impact"),
		UOpenMobileHapticsSampleRecipes::PlayPreparedPattern(
			this,
			TEXT("Combat_Impact"),
			0.55f,
			TEXT("Gameplay")
		)
	);
}

void UOpenMobileHapticsSampleWidget::HandleDamageClicked()
{
	if (Haptics)
	{
		ShowPlaybackResult(
			TEXT("Damage preset"),
			Haptics->PlayGameFeedback(
				EOpenMobileHapticGamePreset::Damage,
				0.55f,
				TEXT("Gameplay")
			)
		);
	}
}

void UOpenMobileHapticsSampleWidget::HandlePickupClicked()
{
	if (Haptics)
	{
		ShowPlaybackResult(
			TEXT("Pickup preset"),
			Haptics->PlayGameFeedback(
				EOpenMobileHapticGamePreset::Pickup,
				0.35f,
				TEXT("Gameplay")
			)
		);
	}
}

void UOpenMobileHapticsSampleWidget::HandleRewardClicked()
{
	ShowPlaybackResult(
		TEXT("Prepared reward"),
		UOpenMobileHapticsSampleRecipes::PlayPreparedPattern(
			this,
			TEXT("Reward_Success"),
			0.5f,
			TEXT("Gameplay")
		)
	);
}

void UOpenMobileHapticsSampleWidget::HandleVehicleClicked()
{
	const FOpenMobileHapticPlaybackResult Result =
		UOpenMobileHapticsSampleRecipes::StartBoundedVehicleFeedback(
			this,
			0.45f
		);
	if (Result.IsAccepted() && Result.Handle.IsValid())
	{
		VehicleHandle = Result.Handle;
		VehicleStartedAtSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				VehicleUpdateTimer,
				this,
				&UOpenMobileHapticsSampleWidget::HandleVehicleUpdate,
				0.25f,
				true
			);
		}
	}
	ShowPlaybackResult(TEXT("Bounded vehicle texture"), Result);
}

void UOpenMobileHapticsSampleWidget::HandleChargingClicked()
{
	if (Haptics)
	{
		ShowPlaybackResult(
			TEXT("Charging connected"),
			Haptics->PlayNotificationFeedback(
				EOpenMobileHapticNotificationType::Success,
				0.3f,
				TEXT("Alerts")
			)
		);
	}
}

void UOpenMobileHapticsSampleWidget::HandleAudioSyncClicked()
{
	const UWorld* World = GetWorld();
	ShowPlaybackResult(
		TEXT("Audio-clock reward"),
		UOpenMobileHapticsSampleRecipes::ScheduleWithAudioClock(
			this,
			TEXT("Reward_Success"),
			World ? World->GetAudioTimeSeconds() : 0.0,
			0.25
		)
	);
}

void UOpenMobileHapticsSampleWidget::HandleAccessibilityClicked()
{
	ShowPlaybackResult(
		TEXT("Accessible confirmation"),
		UOpenMobileHapticsSampleRecipes::PlayAccessibilityConfirmation(
			this,
			0.35f
		)
	);
}

void UOpenMobileHapticsSampleWidget::HandleScheduleClicked()
{
	if (!Haptics)
	{
		return;
	}
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = TEXT("Gameplay");
	Options.Category = TEXT("Reward");
	Options.Priority = EOpenMobileHapticChannelPriority::Low;
	Options.OverlapPolicy = EOpenMobileHapticOverlapPolicy::Queue;
	Options.FallbackPolicy = EOpenMobileHapticFallbackPolicy::Automatic;
	Options.Schedule.Mode = EOpenMobileHapticScheduleMode::Relative;
	Options.Schedule.TimeSeconds = 1.5;
	ShowPlaybackResult(
		TEXT("Scheduled reward"),
		Haptics->PlayNamedPatternAdvanced(
			TEXT("Reward_Success"),
			0.45f,
			Options
		)
	);
}

void UOpenMobileHapticsSampleWidget::HandleCancelClicked()
{
	if (!ActiveHandle.IsValid())
	{
		SetStatus(
			TEXT("No accepted handle is available to cancel."),
			OpenMobileHapticsSampleWidgetPrivate::WarningColor
		);
		return;
	}
	ShowControlResult(
		TEXT("Cancel active handle"),
		UOpenMobileHapticsSampleRecipes::CancelSamplePlayback(
			this,
			ActiveHandle
		)
	);
}

void UOpenMobileHapticsSampleWidget::HandleStopGameplayClicked()
{
	StopVehicleTimer();
	ShowControlResult(
		TEXT("Stop Gameplay channel"),
		UOpenMobileHapticsSampleRecipes::StopSampleChannel(
			this,
			TEXT("Gameplay")
		)
	);
}

void UOpenMobileHapticsSampleWidget::HandleToggleEnabledClicked()
{
	if (Haptics)
	{
		ShowControlResult(
			TEXT("Toggle global enabled"),
			Haptics->SetHapticsEnabled(!Haptics->IsHapticsEnabled())
		);
	}
}

void UOpenMobileHapticsSampleWidget::HandleSofterClicked()
{
	if (Haptics)
	{
		ShowControlResult(
			TEXT("Set master intensity to 35%"),
			Haptics->SetMasterIntensity(0.35f)
		);
	}
}

void UOpenMobileHapticsSampleWidget::HandleFullIntensityClicked()
{
	if (Haptics)
	{
		ShowControlResult(
			TEXT("Set master intensity to 100%"),
			Haptics->SetMasterIntensity(1.0f)
		);
	}
}

void UOpenMobileHapticsSampleWidget::HandleVehicleUpdate()
{
	if (!Haptics || !VehicleHandle.IsValid())
	{
		StopVehicleTimer();
		return;
	}
	const UWorld* World = GetWorld();
	const double Elapsed = World
		? World->GetTimeSeconds() - VehicleStartedAtSeconds
		: 2.0;
	if (Elapsed >= 2.0
		|| OpenMobileHapticsSampleWidgetPrivate::IsTerminal(
			Haptics->GetPlaybackState(VehicleHandle)
		))
	{
		StopVehicleTimer();
		return;
	}
	FOpenMobileHapticDynamicParameterUpdate Update;
	Update.Intensity = 0.3f + 0.15f * FMath::Sin(Elapsed * UE_DOUBLE_PI);
	Update.bUpdateSharpness = true;
	Update.Sharpness = 0.5f + 0.2f * FMath::Sin(Elapsed * UE_DOUBLE_PI * 0.5);
	const FOpenMobileHapticControlResult Result =
		Haptics->UpdatePlaybackParameters(VehicleHandle, Update);
	if (Result.Outcome == EOpenMobileHapticControlOutcome::Unsupported
		|| Result.Outcome == EOpenMobileHapticControlOutcome::StaleHandle)
	{
		StopVehicleTimer();
	}
}

void UOpenMobileHapticsSampleWidget::HandlePlaybackEvent(
	const FOpenMobileHapticPlaybackEvent& Event
)
{
	using namespace OpenMobileHapticsSampleWidgetPrivate;
	RecentEvents.Add(FString::Printf(
		TEXT("%s | %s | %s%s"),
		*EnumName(Event.State),
		Event.PatternOrEffect.IsNone()
			? TEXT("Unnamed")
			: *Event.PatternOrEffect.ToString(),
		Event.Channel.IsNone() ? TEXT("No channel") : *Event.Channel.ToString(),
		Event.Error.IsSet()
			? *FString::Printf(TEXT(" | %s"), *EnumName(Event.Error.Code))
			: TEXT("")
	));
	if (RecentEvents.Num() > 6)
	{
		RecentEvents.RemoveAt(0, RecentEvents.Num() - 6);
	}
	if (EventText)
	{
		EventText->SetText(FText::FromString(
			TEXT("PLAYBACK LIFECYCLE\n") + FString::Join(RecentEvents, TEXT("\n"))
		));
	}
	if (Event.Handle == VehicleHandle && IsTerminal(Event.State))
	{
		StopVehicleTimer();
		VehicleHandle = {};
	}
	if (Event.Handle == ActiveHandle && IsTerminal(Event.State))
	{
		ActiveHandle = {};
	}
	RefreshSummary();
}

void UOpenMobileHapticsSampleWidget::HandleLibrariesPrepared(
	const FOpenMobileHapticLibraryPreloadResult& Result
)
{
	using namespace OpenMobileHapticsSampleWidgetPrivate;
	if (PreloadHandle.IsValid() && !(Result.Handle == PreloadHandle))
	{
		return;
	}
	PreloadHandle = {};
	SetStatus(
		FString::Printf(
			TEXT("Library preparation: %s | %d patterns"),
			*EnumName(Result.Outcome),
			Result.PreparedPatternCount
		),
		Result.Outcome == EOpenMobileHapticLibraryPreloadOutcome::Prepared
			? SuccessColor
			: ErrorColor
	);
	RefreshSummary();
}

#include "OpenMobileHapticsCapabilityTesterWidget.h"

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
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsCapabilityTester.h"
#include "OpenMobileHapticsPreviewProtocol.h"
#include "OpenMobileHapticsSubsystem.h"
#include "TimerManager.h"

namespace OpenMobileHapticsCapabilityTesterWidgetPrivate
{
	constexpr double MaximumTesterDurationSeconds = 0.5;
	constexpr double MinimumPlaybackIntervalSeconds = 0.4;
	constexpr int32 MaximumPlaybackRequestsPerSecond = 2;
	const FName TesterChannel(TEXT("OpenMobileCapabilityTester"));
	const FLinearColor MutedColor(0.72f, 0.77f, 0.85f, 1.0f);
	const FLinearColor SuccessColor(0.42f, 0.94f, 0.67f, 1.0f);
	const FLinearColor ErrorColor(1.0f, 0.42f, 0.42f, 1.0f);

	void SetFontSize(UTextBlock& TextBlock, int32 Size)
	{
		FSlateFontInfo Font = TextBlock.GetFont();
		Font.Size = Size;
		TextBlock.SetFont(Font);
	}

	void AddWithPadding(
		UVerticalBox& Parent,
		UWidget& Child,
		const FMargin& Padding
	)
	{
		UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(&Child);
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}

	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetNameStringByValue(static_cast<int64>(Value))
			: TEXT("Unknown");
	}
}

TSharedRef<SWidget> UOpenMobileHapticsCapabilityTesterWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UOpenMobileHapticsCapabilityTesterWidget::BuildWidgetTree()
{
	using namespace OpenMobileHapticsCapabilityTesterWidgetPrivate;
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("HapticsTesterBackground")
	);
	Background->SetBrushColor(FLinearColor(0.018f, 0.024f, 0.035f, 1.0f));
	Background->SetPadding(FMargin(20.0f));
	WidgetTree->RootWidget = Background;
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("HapticsTesterScroll")
	);
	Background->SetContent(Scroll);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("HapticsTesterColumn")
	);
	Scroll->AddChild(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("HapticsTesterTitle")
	);
	Title->SetText(FText::FromString(TEXT("OpenMobile Haptics Tester")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(
		0.37f,
		0.86f,
		1.0f,
		1.0f
	)));
	SetFontSize(*Title, 28);
	AddWithPadding(*Column, *Title, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	UTextBlock* Warning = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("HapticsTesterWarning")
	);
	Warning->SetText(FText::FromString(TEXT(
		"Development only. Playback is limited to two requests per second, 0.65 intensity, and 0.5 seconds."
	)));
	Warning->SetAutoWrapText(true);
	Warning->SetColorAndOpacity(FSlateColor(MutedColor));
	SetFontSize(*Warning, 14);
	AddWithPadding(*Column, *Warning, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	UUniformGridPanel* Controls =
		WidgetTree->ConstructWidget<UUniformGridPanel>(
			UUniformGridPanel::StaticClass(),
			TEXT("HapticsTesterControls")
		);
	Controls->SetMinDesiredSlotWidth(155.0f);
	Controls->SetMinDesiredSlotHeight(42.0f);
	AddButton(Controls, TEXT("RefreshButton"), TEXT("Refresh"), 0, 0);
	AddButton(Controls, TEXT("CopyButton"), TEXT("Copy snapshot"), 0, 1);
	AddButton(Controls, TEXT("PrepareButton"), TEXT("Prepare resources"), 0, 2);
	AddButton(Controls, TEXT("ReleaseButton"), TEXT("Release resources"), 1, 0);
	AddButton(Controls, TEXT("StopButton"), TEXT("Stop tester"), 1, 1);
	AddButton(Controls, TEXT("SemanticButton"), TEXT("Semantic"), 2, 0);
	AddButton(Controls, TEXT("ImpactButton"), TEXT("Impact"), 2, 1);
	AddButton(Controls, TEXT("NotificationButton"), TEXT("Notification"), 2, 2);
	AddButton(Controls, TEXT("PresetButton"), TEXT("Game preset"), 3, 0);
	AddButton(Controls, TEXT("PulseButton"), TEXT("Basic pulse"), 3, 1);
	AddButton(Controls, TEXT("WaveformButton"), TEXT("Waveform asset"), 4, 0);
	AddButton(Controls, TEXT("PrimitiveButton"), TEXT("Primitive asset"), 4, 1);
	AddButton(Controls, TEXT("EnvelopeButton"), TEXT("Envelope asset"), 4, 2);
	AddButton(Controls, TEXT("AHAPButton"), TEXT("AHAP asset"), 5, 0);
	AddButton(Controls, TEXT("FallbackButton"), TEXT("Fallback asset"), 5, 1);
	AddWithPadding(*Column, *Controls, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("HapticsTesterStatus")
	);
	StatusText->SetAutoWrapText(true);
	StatusText->SetColorAndOpacity(FSlateColor(MutedColor));
	SetFontSize(*StatusText, 14);
	AddWithPadding(*Column, *StatusText, FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	SnapshotText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("HapticsTesterSnapshot")
	);
	SnapshotText->SetAutoWrapText(true);
	SnapshotText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetFontSize(*SnapshotText, 12);
	AddWithPadding(*Column, *SnapshotText, FMargin(0.0f, 0.0f, 0.0f, 20.0f));
}

UButton* UOpenMobileHapticsCapabilityTesterWidget::AddButton(
	UUniformGridPanel* Grid,
	FName Name,
	const FString& Label,
	int32 Row,
	int32 Column
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
	OpenMobileHapticsCapabilityTesterWidgetPrivate::SetFontSize(*Text, 14);
	Button->SetContent(Text);
	UUniformGridSlot* Slot = Grid->AddChildToUniformGrid(Button, Row, Column);
	Slot->SetHorizontalAlignment(HAlign_Fill);
	Slot->SetVerticalAlignment(VAlign_Fill);
	Buttons.Add(Button);
	return Button;
}

void UOpenMobileHapticsCapabilityTesterWidget::NativeConstruct()
{
	Super::NativeConstruct();
#if OPENMOBILE_HAPTICS_TESTER_ENABLED
	Haptics = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOpenMobileHapticsSubsystem>()
		: nullptr;
	if (!Haptics || Buttons.Num() != 15)
	{
		SetStatus(
			TEXT("Haptics tester could not initialize."),
			OpenMobileHapticsCapabilityTesterWidgetPrivate::ErrorColor
		);
		return;
	}
	Buttons[0]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRefresh);
	Buttons[1]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCopySnapshot);
	Buttons[2]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePrepare);
	Buttons[3]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRelease);
	Buttons[4]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStop);
	Buttons[5]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSemantic);
	Buttons[6]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleImpact);
	Buttons[7]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNotification);
	Buttons[8]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePreset);
	Buttons[9]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePulse);
	Buttons[10]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleWaveform);
	Buttons[11]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePrimitive);
	Buttons[12]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEnvelope);
	Buttons[13]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAHAP);
	Buttons[14]->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFallback);
	Haptics->OnNamedLibrariesPrepared.AddUniqueDynamic(
		this,
		&ThisClass::HandlePrepared
	);
	SetStatus(
		TEXT("Ready. Assign short non-looping assets in a widget subclass."),
		OpenMobileHapticsCapabilityTesterWidgetPrivate::MutedColor
	);
	RefreshCapabilitySnapshot();
#else
	SetStatus(
		TEXT("Capability tester is available only in Development builds."),
		OpenMobileHapticsCapabilityTesterWidgetPrivate::ErrorColor
	);
#endif
}

void UOpenMobileHapticsCapabilityTesterWidget::NativeDestruct()
{
#if OPENMOBILE_HAPTICS_TESTER_ENABLED
	StopTesterPlayback();
	if (Haptics)
	{
		Haptics->OnNamedLibrariesPrepared.RemoveAll(this);
	}
	Haptics = nullptr;
#endif
	Super::NativeDestruct();
}

bool UOpenMobileHapticsCapabilityTesterWidget::AdmitPlayback(
	const FString& Label
)
{
#if OPENMOBILE_HAPTICS_TESTER_ENABLED
	using namespace OpenMobileHapticsCapabilityTesterWidgetPrivate;
	if (!Haptics)
	{
		SetStatus(TEXT("Haptics subsystem is unavailable."), ErrorColor);
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	RecentPlaybackTimes.RemoveAll(
		[Now](double Time) { return Now - Time >= 1.0; }
	);
	if (RecentPlaybackTimes.Num() >= MaximumPlaybackRequestsPerSecond
		|| (LastPlaybackSeconds >= 0.0
			&& Now - LastPlaybackSeconds < MinimumPlaybackIntervalSeconds))
	{
		SetStatus(Label + TEXT(": tester rate limit reached."), ErrorColor);
		return false;
	}
	RecentPlaybackTimes.Add(Now);
	LastPlaybackSeconds = Now;
	StopTesterPlayback();
	return true;
#else
	static_cast<void>(Label);
	return false;
#endif
}

void UOpenMobileHapticsCapabilityTesterWidget::HandlePlaybackResult(
	const FString& Label,
	const FOpenMobileHapticPlaybackResult& Result
)
{
	using namespace OpenMobileHapticsCapabilityTesterWidgetPrivate;
	FString Detail = Result.IsAccepted()
		? FString::Printf(
			TEXT("%s: %s through %s."),
			*Label,
			*EnumName(Result.Outcome),
			*Result.ResolvedPath.ToString()
		)
		: FString::Printf(
			TEXT("%s: %s. %s"),
			*Label,
			*EnumName(Result.Outcome),
			*Result.Error.Message
		);
	Detail = FOpenMobileHapticsPreviewProtocol::SanitizeText(Detail, 160);
	SetStatus(Detail, Result.IsAccepted() ? SuccessColor : ErrorColor);
	if (Result.IsAccepted() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			SafetyStopTimer,
			this,
			&ThisClass::StopTesterPlayback,
			MaximumTesterDurationSeconds,
			false
		);
	}
	RefreshCapabilitySnapshot();
}

void UOpenMobileHapticsCapabilityTesterWidget::PlayConfiguredPattern(
	const FString& Label,
	const TSoftObjectPtr<UOpenMobileHapticPatternAsset>& Pattern
)
{
#if OPENMOBILE_HAPTICS_TESTER_ENABLED
	if (!AdmitPlayback(Label))
	{
		return;
	}
	UOpenMobileHapticPatternAsset* Asset = Pattern.LoadSynchronous();
	if (!Asset)
	{
		SetStatus(
			Label + TEXT(": assign a pattern asset first."),
			OpenMobileHapticsCapabilityTesterWidgetPrivate::ErrorColor
		);
		return;
	}
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel =
		OpenMobileHapticsCapabilityTesterWidgetPrivate::TesterChannel;
	Options.Category = TEXT("Diagnostics");
	Options.Priority = EOpenMobileHapticChannelPriority::Low;
	Options.OverlapPolicy = EOpenMobileHapticOverlapPolicy::Replace;
	Options.FallbackPolicy = Asset->FallbackPolicy;
	HandlePlaybackResult(
		Label,
		Haptics->SubmitCapabilityTestPattern(Asset, Options)
	);
#else
	static_cast<void>(Label);
	static_cast<void>(Pattern);
#endif
}

void UOpenMobileHapticsCapabilityTesterWidget::StopTesterPlayback()
{
#if OPENMOBILE_HAPTICS_TESTER_ENABLED
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SafetyStopTimer);
	}
	if (Haptics)
	{
		Haptics->StopChannel(
			OpenMobileHapticsCapabilityTesterWidgetPrivate::TesterChannel
		);
	}
#endif
}

void UOpenMobileHapticsCapabilityTesterWidget::SetStatus(
	const FString& Value,
	const FLinearColor& Color
)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Value));
		StatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UOpenMobileHapticsCapabilityTesterWidget::RefreshCapabilitySnapshot()
{
	if (!Haptics || !SnapshotText)
	{
		return;
	}
	const FOpenMobileHapticsCapabilityTesterSnapshot Snapshot =
		FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Capture(*Haptics);
	FString Error;
	if (!FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Serialize(
		Snapshot,
		SnapshotJson,
		Error
	))
	{
		SnapshotJson.Reset();
		SetStatus(
			Error,
			OpenMobileHapticsCapabilityTesterWidgetPrivate::ErrorColor
		);
	}
	SnapshotText->SetText(FText::FromString(
		FOpenMobileHapticsCapabilityTesterSnapshotBuilder::ToDisplayText(
			Snapshot
		)
	));
}

FString UOpenMobileHapticsCapabilityTesterWidget::
GetSanitizedSnapshotJson() const
{
	return SnapshotJson;
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleRefresh()
{
	RefreshCapabilitySnapshot();
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleCopySnapshot()
{
	RefreshCapabilitySnapshot();
	if (SnapshotJson.IsEmpty())
	{
		return;
	}
	FPlatformApplicationMisc::ClipboardCopy(*SnapshotJson);
	SetStatus(
		TEXT("Sanitized capability snapshot copied."),
		OpenMobileHapticsCapabilityTesterWidgetPrivate::SuccessColor
	);
}

void UOpenMobileHapticsCapabilityTesterWidget::HandlePrepare()
{
	if (Haptics)
	{
		Haptics->PreloadNamedLibraries();
		SetStatus(
			TEXT("Preparing configured resources."),
			OpenMobileHapticsCapabilityTesterWidgetPrivate::MutedColor
		);
	}
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleRelease()
{
	if (Haptics)
	{
		StopTesterPlayback();
		Haptics->ReleaseNamedLibraries();
		RefreshCapabilitySnapshot();
	}
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleStop()
{
	StopTesterPlayback();
	SetStatus(
		TEXT("Tester playback stopped."),
		OpenMobileHapticsCapabilityTesterWidgetPrivate::MutedColor
	);
	RefreshCapabilitySnapshot();
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleSemantic()
{
	if (AdmitPlayback(TEXT("Semantic")))
	{
		HandlePlaybackResult(TEXT("Semantic"), Haptics->PlaySemanticFeedback(
			SemanticEffect,
			0.65f,
			OpenMobileHapticsCapabilityTesterWidgetPrivate::TesterChannel
		));
	}
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleImpact()
{
	if (AdmitPlayback(TEXT("Impact")))
	{
		HandlePlaybackResult(TEXT("Impact"), Haptics->PlayImpactFeedback(
			ImpactStyle,
			0.65f,
			OpenMobileHapticsCapabilityTesterWidgetPrivate::TesterChannel
		));
	}
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleNotification()
{
	if (AdmitPlayback(TEXT("Notification")))
	{
		HandlePlaybackResult(
			TEXT("Notification"),
			Haptics->PlayNotificationFeedback(
				NotificationType,
				0.65f,
				OpenMobileHapticsCapabilityTesterWidgetPrivate::TesterChannel
			)
		);
	}
}

void UOpenMobileHapticsCapabilityTesterWidget::HandlePreset()
{
	if (AdmitPlayback(TEXT("Game preset")))
	{
		HandlePlaybackResult(TEXT("Game preset"), Haptics->PlayGameFeedback(
			GamePreset,
			0.65f,
			OpenMobileHapticsCapabilityTesterWidgetPrivate::TesterChannel
		));
	}
}

void UOpenMobileHapticsCapabilityTesterWidget::HandlePulse()
{
	if (AdmitPlayback(TEXT("Basic pulse")))
	{
		HandlePlaybackResult(TEXT("Basic pulse"), Haptics->Vibrate(
			0.05f,
			0.65f,
			OpenMobileHapticsCapabilityTesterWidgetPrivate::TesterChannel
		));
	}
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleWaveform()
{
	PlayConfiguredPattern(TEXT("Waveform"), WaveformPattern);
}

void UOpenMobileHapticsCapabilityTesterWidget::HandlePrimitive()
{
	PlayConfiguredPattern(TEXT("Primitive"), PrimitivePattern);
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleEnvelope()
{
	PlayConfiguredPattern(TEXT("Envelope"), EnvelopePattern);
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleAHAP()
{
	PlayConfiguredPattern(TEXT("AHAP"), AHAPPattern);
}

void UOpenMobileHapticsCapabilityTesterWidget::HandleFallback()
{
	PlayConfiguredPattern(TEXT("Fallback"), FallbackPattern);
}

void UOpenMobileHapticsCapabilityTesterWidget::HandlePrepared(
	const FOpenMobileHapticLibraryPreloadResult& Result
)
{
	SetStatus(
		FString::Printf(
			TEXT("Preparation %s with %d patterns."),
			*OpenMobileHapticsCapabilityTesterWidgetPrivate::EnumName(
				Result.Outcome
			),
			Result.PreparedPatternCount
		),
		Result.Outcome == EOpenMobileHapticLibraryPreloadOutcome::Prepared
			? OpenMobileHapticsCapabilityTesterWidgetPrivate::SuccessColor
			: OpenMobileHapticsCapabilityTesterWidgetPrivate::ErrorColor
	);
	RefreshCapabilitySnapshot();
}

#include "OpenMobileDeviceDemoWidget.h"

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
#include "OpenMobileDeviceBlueprintExamples.h"
#include "OpenMobileDeviceBlueprintLibrary.h"
#include "OpenMobileDeviceEndpointReachabilityAsyncAction.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceUserInitiatedPasteAsyncAction.h"
#include "OpenMobileDeviceUserInitiatedPasteTypes.h"

namespace OpenMobileDeviceDemoPrivate
{
	const FLinearColor MutedColor(0.74f, 0.78f, 0.86f, 1.0f);
	const FLinearColor SuccessColor(0.42f, 0.94f, 0.67f, 1.0f);
	const FLinearColor WarningColor(1.0f, 0.70f, 0.32f, 1.0f);
	const FLinearColor ErrorColor(1.0f, 0.42f, 0.42f, 1.0f);

	void SetFontSize(UTextBlock* TextBlock, const int32 Size)
	{
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = Size;
		TextBlock->SetFont(Font);
	}

	void AddWithPadding(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding)
	{
		UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}

	UButton* AddButton(
		UWidgetTree* WidgetTree,
		UUniformGridPanel* Grid,
		const FName Name,
		const FString& Label,
		const int32 Row,
		const int32 Column
	)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*(Name.ToString() + TEXT("Label")))
		);
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		SetFontSize(Text, 16);
		Button->SetContent(Text);
		UUniformGridSlot* Slot = Grid->AddChildToUniformGrid(Button, Row, Column);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
		return Button;
	}

	template <typename EnumType>
	FString EnumName(const EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetDisplayNameTextByValue(static_cast<int64>(Value)).ToString()
			: TEXT("Unknown");
	}

	FString OptionalFloat(const FOpenMobileDeviceOptionalFloat& Value, const FString& Suffix)
	{
		return Value.bIsAvailable
			? FString::Printf(TEXT("%.1f%s"), Value.Value, *Suffix)
			: TEXT("Unavailable");
	}

	FString OptionalBool(const FOpenMobileDeviceOptionalBool& Value)
	{
		return Value.bIsAvailable
			? (Value.Value ? TEXT("Yes") : TEXT("No"))
			: TEXT("Unavailable");
	}

	FString OptionalBytes(const FOpenMobileDeviceOptionalInt64& Value)
	{
		return Value.bIsAvailable
			? UOpenMobileDeviceBlueprintLibrary::FormatByteCount(Value.Value).ToString()
			: TEXT("Unavailable");
	}

	FString OptionalString(const FOpenMobileDeviceOptionalString& Value)
	{
		return Value.bIsAvailable && !Value.Value.IsEmpty()
			? Value.Value
			: TEXT("Unavailable");
	}

	FString CapabilityStateLabel(const FOpenMobileDeviceCapability& Capability)
	{
		FString Label;
		switch (Capability.State)
		{
		case EOpenMobileCapabilityState::Available:
			Label = TEXT("Available");
			break;
		case EOpenMobileCapabilityState::NotSupported:
			Label = TEXT("Unsupported");
			break;
		case EOpenMobileCapabilityState::PermissionRequired:
			Label = TEXT("Permission required");
			break;
		case EOpenMobileCapabilityState::Denied:
			Label = TEXT("Permission denied");
			break;
		case EOpenMobileCapabilityState::Restricted:
			Label = TEXT("Restricted");
			break;
		default:
			Label = EnumName(Capability.State);
			break;
		}

		if (Capability.Limit == EOpenMobileDeviceCapabilityLimit::Simulator)
		{
			Label += TEXT(" | Simulator");
		}
		if (Capability.BackendName.ToString().Contains(TEXT("Mock")))
		{
			Label += TEXT(" | Mock");
		}
		return Label;
	}
}

TSharedRef<SWidget> UOpenMobileDeviceDemoWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return Super::RebuildWidget();
}

void UOpenMobileDeviceDemoWidget::BuildWidgetTree()
{
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("DeviceBackground")
	);
	Background->SetBrushColor(FLinearColor(0.018f, 0.024f, 0.035f, 1.0f));
	Background->SetPadding(FMargin(24.0f));
	WidgetTree->RootWidget = Background;

	UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("DeviceScrollBox")
	);
	Background->SetContent(ScrollBox);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("DeviceColumn")
	);
	ScrollBox->AddChild(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("DeviceTitle")
	);
	Title->SetText(FText::FromString(TEXT("OpenMobile Device")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.37f, 0.86f, 1.0f, 1.0f)));
	OpenMobileDeviceDemoPrivate::SetFontSize(Title, 30);
	OpenMobileDeviceDemoPrivate::AddWithPadding(Column, Title, FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("DeviceSubtitle")
	);
	Subtitle->SetText(FText::FromString(TEXT(
		"Focused snapshots, monitored events, capability reasons, controls, and explicit async actions"
	)));
	Subtitle->SetColorAndOpacity(FSlateColor(OpenMobileDeviceDemoPrivate::MutedColor));
	Subtitle->SetAutoWrapText(true);
	OpenMobileDeviceDemoPrivate::SetFontSize(Subtitle, 16);
	OpenMobileDeviceDemoPrivate::AddWithPadding(Column, Subtitle, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	UUniformGridPanel* Controls = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass(),
		TEXT("DeviceControls")
	);
	Controls->SetMinDesiredSlotWidth(180.0f);
	Controls->SetMinDesiredSlotHeight(44.0f);
	RefreshButton = OpenMobileDeviceDemoPrivate::AddButton(
		WidgetTree, Controls, TEXT("RefreshButton"), TEXT("Refresh snapshots"), 0, 0
	);
	ReachabilityButton = OpenMobileDeviceDemoPrivate::AddButton(
		WidgetTree, Controls, TEXT("ReachabilityButton"), TEXT("Test HTTPS"), 0, 1
	);
	PasteButton = OpenMobileDeviceDemoPrivate::AddButton(
		WidgetTree, Controls, TEXT("PasteButton"), TEXT("Paste text"), 1, 0
	);
	BrightnessButton = OpenMobileDeviceDemoPrivate::AddButton(
		WidgetTree, Controls, TEXT("BrightnessButton"), TEXT("Hold brightness"), 1, 1
	);
	AwakeButton = OpenMobileDeviceDemoPrivate::AddButton(
		WidgetTree, Controls, TEXT("AwakeButton"), TEXT("Keep awake"), 2, 0
	);
	SettingsButton = OpenMobileDeviceDemoPrivate::AddButton(
		WidgetTree, Controls, TEXT("SettingsButton"), TEXT("Open app settings"), 2, 1
	);
	OpenMobileDeviceDemoPrivate::AddWithPadding(Column, Controls, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	ActionStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ActionStatus")
	);
	ActionStatusText->SetText(FText::FromString(TEXT("Ready. Monitoring starts with this page and stops when it closes.")));
	ActionStatusText->SetColorAndOpacity(FSlateColor(OpenMobileDeviceDemoPrivate::MutedColor));
	ActionStatusText->SetAutoWrapText(true);
	OpenMobileDeviceDemoPrivate::SetFontSize(ActionStatusText, 15);
	OpenMobileDeviceDemoPrivate::AddWithPadding(Column, ActionStatusText, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	SnapshotText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("FocusedSnapshots")
	);
	SnapshotText->SetAutoWrapText(true);
	SnapshotText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	OpenMobileDeviceDemoPrivate::SetFontSize(SnapshotText, 15);
	OpenMobileDeviceDemoPrivate::AddWithPadding(Column, SnapshotText, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	PolicyText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("PolicyExamples")
	);
	PolicyText->SetAutoWrapText(true);
	PolicyText->SetColorAndOpacity(FSlateColor(OpenMobileDeviceDemoPrivate::MutedColor));
	OpenMobileDeviceDemoPrivate::SetFontSize(PolicyText, 15);
	OpenMobileDeviceDemoPrivate::AddWithPadding(Column, PolicyText, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	UTextBlock* BlueprintRecipes = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BlueprintRecipes")
	);
	BlueprintRecipes->SetText(FText::FromString(TEXT(
		"BLUEPRINT RECIPES\n"
		"Construct: Start Monitoring Subscription, promote the return value to a widget variable.\n"
		"Destruct: Stop Device Monitoring and release retained brightness or awake handles.\n"
		"Power event: Should Reduce Quality for Power selects full or reduced effects.\n"
		"Storage or network event: Should Allow Download gates optional content.\n"
		"Network event: Is Network Handoff decides when to retry app-owned work.\n"
		"Window event: Should Rebuild Safe Area updates game-owned layout.\n"
		"Denied or restricted capability: Should Recover Through Settings, then Open Application Settings."
	)));
	BlueprintRecipes->SetAutoWrapText(true);
	BlueprintRecipes->SetColorAndOpacity(FSlateColor(OpenMobileDeviceDemoPrivate::MutedColor));
	OpenMobileDeviceDemoPrivate::SetFontSize(BlueprintRecipes, 14);
	OpenMobileDeviceDemoPrivate::AddWithPadding(Column, BlueprintRecipes, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	CapabilityText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("CapabilityDiagnostics")
	);
	CapabilityText->SetAutoWrapText(true);
	CapabilityText->SetColorAndOpacity(FSlateColor(OpenMobileDeviceDemoPrivate::MutedColor));
	OpenMobileDeviceDemoPrivate::SetFontSize(CapabilityText, 14);
	OpenMobileDeviceDemoPrivate::AddWithPadding(Column, CapabilityText, FMargin(0.0f, 0.0f, 0.0f, 24.0f));
}

void UOpenMobileDeviceDemoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleRefreshClicked);
	ReachabilityButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleReachabilityClicked);
	PasteButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandlePasteClicked);
	BrightnessButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleBrightnessClicked);
	AwakeButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleAwakeClicked);
	SettingsButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleSettingsClicked);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		DeviceSubsystem = GameInstance->GetSubsystem<UOpenMobileDeviceSubsystem>();
	}

	if (!DeviceSubsystem)
	{
		SetActionStatus(TEXT("Device subsystem unavailable."), OpenMobileDeviceDemoPrivate::ErrorColor);
		return;
	}

	BindSubsystemEvents();
	const TArray<EOpenMobileDeviceMonitoringGroup> Groups = {
		EOpenMobileDeviceMonitoringGroup::Power,
		EOpenMobileDeviceMonitoringGroup::MemoryPressure,
		EOpenMobileDeviceMonitoringGroup::Storage,
		EOpenMobileDeviceMonitoringGroup::Network,
		EOpenMobileDeviceMonitoringGroup::WindowDisplay,
		EOpenMobileDeviceMonitoringGroup::Appearance,
		EOpenMobileDeviceMonitoringGroup::Accessibility
	};
	MonitoringSubscription = DeviceSubsystem->StartMonitoring(this, Groups);
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::NativeDestruct()
{
	UnbindSubsystemEvents();

	if (MonitoringSubscription)
	{
		MonitoringSubscription->Stop();
		MonitoringSubscription = nullptr;
	}
	if (ReachabilityAction && !ReachabilityAction->IsFinished())
	{
		ReachabilityAction->Cancel();
	}
	if (PasteAction && !PasteAction->IsFinished())
	{
		PasteAction->Cancel();
	}
	if (BrightnessHandle)
	{
		BrightnessHandle->Release();
		BrightnessHandle = nullptr;
	}
	if (AwakeHandle)
	{
		AwakeHandle->Release();
		AwakeHandle = nullptr;
	}
	ReachabilityAction = nullptr;
	PasteAction = nullptr;
	DeviceSubsystem = nullptr;
	Super::NativeDestruct();
}

void UOpenMobileDeviceDemoWidget::BindSubsystemEvents()
{
	DeviceSubsystem->OnPowerSnapshotChanged.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandlePowerChanged);
	DeviceSubsystem->OnMemorySnapshotChanged.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleMemoryChanged);
	DeviceSubsystem->OnStorageSnapshotChanged.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleStorageChanged);
	DeviceSubsystem->OnNetworkPathSnapshotChanged.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleNetworkChanged);
	DeviceSubsystem->OnWindowDisplaySnapshotChanged.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleWindowChanged);
	DeviceSubsystem->OnAppearanceSnapshotChanged.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleAppearanceChanged);
	DeviceSubsystem->OnAccessibilitySnapshotChanged.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleAccessibilityChanged);
	DeviceSubsystem->OnApplicationSettingsReturned.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleSettingsReturned);
}

void UOpenMobileDeviceDemoWidget::UnbindSubsystemEvents()
{
	if (!DeviceSubsystem)
	{
		return;
	}

	DeviceSubsystem->OnPowerSnapshotChanged.RemoveDynamic(this, &UOpenMobileDeviceDemoWidget::HandlePowerChanged);
	DeviceSubsystem->OnMemorySnapshotChanged.RemoveDynamic(this, &UOpenMobileDeviceDemoWidget::HandleMemoryChanged);
	DeviceSubsystem->OnStorageSnapshotChanged.RemoveDynamic(this, &UOpenMobileDeviceDemoWidget::HandleStorageChanged);
	DeviceSubsystem->OnNetworkPathSnapshotChanged.RemoveDynamic(this, &UOpenMobileDeviceDemoWidget::HandleNetworkChanged);
	DeviceSubsystem->OnWindowDisplaySnapshotChanged.RemoveDynamic(this, &UOpenMobileDeviceDemoWidget::HandleWindowChanged);
	DeviceSubsystem->OnAppearanceSnapshotChanged.RemoveDynamic(this, &UOpenMobileDeviceDemoWidget::HandleAppearanceChanged);
	DeviceSubsystem->OnAccessibilitySnapshotChanged.RemoveDynamic(this, &UOpenMobileDeviceDemoWidget::HandleAccessibilityChanged);
	DeviceSubsystem->OnApplicationSettingsReturned.RemoveDynamic(this, &UOpenMobileDeviceDemoWidget::HandleSettingsReturned);
}

void UOpenMobileDeviceDemoWidget::RefreshSnapshots()
{
	if (!DeviceSubsystem)
	{
		return;
	}

	const FOpenMobileDeviceInformationSnapshot Device = DeviceSubsystem->GetDeviceInformationSnapshot();
	const FOpenMobileApplicationMetadataSnapshot Application = DeviceSubsystem->GetApplicationMetadataSnapshot();
	const FOpenMobileLocaleSnapshot Locale = DeviceSubsystem->GetLocaleSnapshot();
	const FOpenMobilePowerSnapshot Power = DeviceSubsystem->GetPowerSnapshot();
	const FOpenMobileMediaVolumeSnapshot Volume = DeviceSubsystem->GetMediaVolumeSnapshot();
	const FOpenMobileMemorySnapshot Memory = DeviceSubsystem->GetMemorySnapshot();
	const FOpenMobileStorageSnapshot Storage = DeviceSubsystem->GetStorageSnapshot();
	const FOpenMobileNetworkPathSnapshot Network = DeviceSubsystem->GetNetworkPathSnapshot();
	const FOpenMobileWindowDisplaySnapshot Window = DeviceSubsystem->GetWindowDisplaySnapshot();
	const FOpenMobileBrightnessSnapshot Brightness = DeviceSubsystem->GetBrightnessSnapshot();
	const FOpenMobileFlashlightSnapshot Flashlight = DeviceSubsystem->GetFlashlightSnapshot();
	const FOpenMobileAppearanceSnapshot Appearance = DeviceSubsystem->GetAppearanceSnapshot();
	const FOpenMobileAccessibilitySnapshot Accessibility = DeviceSubsystem->GetAccessibilitySnapshot();

	const FString WindowSize = Window.bLogicalWindowSizeAvailable
		? FString::Printf(TEXT("%.0f x %.0f"), Window.LogicalWindowSize.X, Window.LogicalWindowSize.Y)
		: TEXT("Unavailable");
	const FString SafeArea = Window.SafeAreaInsets.bIsAvailable
		? FString::Printf(
			TEXT("L%.0f T%.0f R%.0f B%.0f"),
			Window.SafeAreaInsets.Left,
			Window.SafeAreaInsets.Top,
			Window.SafeAreaInsets.Right,
			Window.SafeAreaInsets.Bottom
		)
		: TEXT("Unavailable");

	SnapshotText->SetText(FText::FromString(FString::Printf(
		TEXT("FOCUSED SNAPSHOTS\n")
		TEXT("Device: %s | %s | %s | emulator %s\n")
		TEXT("App: %s | version %s | build %s\n")
		TEXT("Locale: %s | time zone %s\n")
		TEXT("Power: battery %s | saving %s | thermal %s\n")
		TEXT("Media volume: %s\n")
		TEXT("Memory: available %s | pressure %s\n")
		TEXT("Storage: available %s | low %s\n")
		TEXT("Network: %s | transport %s | metered %s\n")
		TEXT("Window: %s | safe area %s | %s\n")
		TEXT("Brightness: %s | flashlight %s\n")
		TEXT("Appearance: %s | text scale %s | reduced animation %s"),
		*OpenMobileDeviceDemoPrivate::EnumName(Device.Platform),
		*OpenMobileDeviceDemoPrivate::OptionalString(Device.Model),
		*OpenMobileDeviceDemoPrivate::OptionalString(Device.ReadableOsVersion),
		*OpenMobileDeviceDemoPrivate::EnumName(Device.EmulatorConfidence),
		*OpenMobileDeviceDemoPrivate::OptionalString(Application.DisplayName),
		*OpenMobileDeviceDemoPrivate::OptionalString(Application.VersionName),
		*OpenMobileDeviceDemoPrivate::OptionalString(Application.BuildNumber),
		*OpenMobileDeviceDemoPrivate::OptionalString(Locale.LocaleIdentifier),
		*OpenMobileDeviceDemoPrivate::OptionalString(Locale.TimeZoneIdentifier),
		*OpenMobileDeviceDemoPrivate::OptionalFloat(Power.BatteryPercent, TEXT("%")),
		*OpenMobileDeviceDemoPrivate::OptionalBool(Power.bPowerSavingEnabled),
		*OpenMobileDeviceDemoPrivate::EnumName(Power.ThermalState),
		*OpenMobileDeviceDemoPrivate::OptionalFloat(Volume.VolumePercent, TEXT("%")),
		*OpenMobileDeviceDemoPrivate::OptionalBytes(Memory.AvailablePhysicalBytes),
		*OpenMobileDeviceDemoPrivate::EnumName(Memory.PressureState),
		*OpenMobileDeviceDemoPrivate::OptionalBytes(Storage.AvailableBytes),
		*OpenMobileDeviceDemoPrivate::OptionalBool(Storage.bIsLowStorage),
		*OpenMobileDeviceDemoPrivate::EnumName(Network.PathState),
		Network.bDefaultTransportAvailable
			? *OpenMobileDeviceDemoPrivate::EnumName(Network.DefaultTransport)
			: TEXT("Unavailable"),
		*OpenMobileDeviceDemoPrivate::OptionalBool(Network.bIsMetered),
		*WindowSize,
		*SafeArea,
		*OpenMobileDeviceDemoPrivate::EnumName(Window.Orientation),
		*OpenMobileDeviceDemoPrivate::OptionalFloat(Brightness.CurrentBrightness, TEXT("")),
		*OpenMobileDeviceDemoPrivate::EnumName(Flashlight.HardwareState),
		*OpenMobileDeviceDemoPrivate::EnumName(Appearance.Appearance),
		*OpenMobileDeviceDemoPrivate::OptionalFloat(Accessibility.PreferredTextScale, TEXT("x")),
		*OpenMobileDeviceDemoPrivate::OptionalBool(Accessibility.bReducedAnimationPreferred)
	)));

	const bool bReduceQuality = UOpenMobileDeviceBlueprintExamples::ShouldReduceQualityForPower(Power);
	const bool bAllowDownload = UOpenMobileDeviceBlueprintExamples::ShouldAllowDownload(
		Storage,
		Network,
		100 * 1024 * 1024
	);
	PolicyText->SetText(FText::FromString(FString::Printf(
		TEXT("BLUEPRINT POLICY EXAMPLES\n")
		TEXT("Quality policy: %s\n")
		TEXT("100 MiB download: %s\n")
		TEXT("Monitoring subscription: %s | brightness handle: %s | awake handle: %s"),
		bReduceQuality ? TEXT("reduced") : TEXT("full"),
		bAllowDownload ? TEXT("allowed") : TEXT("gated"),
		MonitoringSubscription && MonitoringSubscription->IsActive() ? TEXT("active") : TEXT("stopped"),
		BrightnessHandle && BrightnessHandle->IsActive() ? TEXT("active") : TEXT("released"),
		AwakeHandle && AwakeHandle->IsActive() ? TEXT("active") : TEXT("released")
	)));

	UpdateCapabilityReport(UOpenMobileDeviceBlueprintLibrary::GetDeviceCapabilityReport());
}

void UOpenMobileDeviceDemoWidget::UpdateCapabilityReport(
	const FOpenMobileDeviceCapabilityReport& Report
)
{
	int32 AvailableCount = 0;
	int32 UnsupportedCount = 0;
	int32 RestrictedCount = 0;
	int32 PermissionCount = 0;
	int32 SimulatorCount = 0;
	FName BackendName = NAME_None;
	TArray<FString> Reasons;

	for (const FOpenMobileDeviceCapability& Capability : Report.Capabilities)
	{
		if (!Capability.BackendName.IsNone())
		{
			BackendName = Capability.BackendName;
		}
		AvailableCount += Capability.State == EOpenMobileCapabilityState::Available ? 1 : 0;
		UnsupportedCount += Capability.State == EOpenMobileCapabilityState::NotSupported ? 1 : 0;
		RestrictedCount += Capability.State == EOpenMobileCapabilityState::Restricted ? 1 : 0;
		PermissionCount += Capability.State == EOpenMobileCapabilityState::Denied
			|| Capability.State == EOpenMobileCapabilityState::PermissionRequired ? 1 : 0;
		SimulatorCount += Capability.Limit == EOpenMobileDeviceCapabilityLimit::Simulator ? 1 : 0;

		if (!Capability.IsAvailable()
			|| Capability.Limit == EOpenMobileDeviceCapabilityLimit::Simulator
			|| Capability.BackendName.ToString().Contains(TEXT("Mock")))
		{
			FString Reason = FString::Printf(
				TEXT("%s: %s"),
				*Capability.Name.ToString(),
				*OpenMobileDeviceDemoPrivate::CapabilityStateLabel(Capability)
			);
			if (!Capability.Detail.IsEmpty())
			{
				Reason += TEXT(" | ") + Capability.Detail;
			}
			Reasons.Add(MoveTemp(Reason));
		}
	}

	const FString ReasonText = Reasons.IsEmpty()
		? TEXT("All reported capabilities are available.")
		: FString::Join(Reasons, TEXT("\n"));
	CapabilityText->SetText(FText::FromString(FString::Printf(
		TEXT("CAPABILITY REASONS AND DIAGNOSTICS\n")
		TEXT("Backend %s | available %d | Unsupported %d | Restricted %d | Permission denied or required %d | Simulator %d\n")
		TEXT("Mock states can be injected from OpenMobile Device Mock settings in the editor.\n%s"),
		BackendName.IsNone() ? TEXT("None") : *BackendName.ToString(),
		AvailableCount,
		UnsupportedCount,
		RestrictedCount,
		PermissionCount,
		SimulatorCount,
		*ReasonText
	)));
}

void UOpenMobileDeviceDemoWidget::SetActionStatus(
	const FString& Message,
	const FLinearColor& Color
)
{
	if (ActionStatusText)
	{
		ActionStatusText->SetText(FText::FromString(Message));
		ActionStatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UOpenMobileDeviceDemoWidget::HandleRefreshClicked()
{
	RefreshSnapshots();
	SetActionStatus(TEXT("Snapshots and diagnostics refreshed."), OpenMobileDeviceDemoPrivate::SuccessColor);
}

void UOpenMobileDeviceDemoWidget::HandleReachabilityClicked()
{
	if (ReachabilityAction && !ReachabilityAction->IsFinished())
	{
		return;
	}

	FOpenMobileEndpointReachabilityOptions Options;
	Options.TimeoutSeconds = 5.0f;
	ReachabilityAction = UOpenMobileDeviceEndpointReachabilityAsyncAction::TestEndpointReachability(
		this,
		TEXT("https://example.com"),
		Options
	);
	ReachabilityAction->Completed.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleReachabilityCompleted);
	ReachabilityAction->Cancelled.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleReachabilityCancelled);
	ReachabilityAction->Failed.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandleReachabilityFailed);
	SetActionStatus(TEXT("Testing https://example.com once..."), OpenMobileDeviceDemoPrivate::MutedColor);
	ReachabilityAction->Activate();
}

void UOpenMobileDeviceDemoWidget::HandlePasteClicked()
{
	if (PasteAction && !PasteAction->IsFinished())
	{
		return;
	}

	FOpenMobileUserInitiatedPasteRequest Request;
	Request.ContentType = EOpenMobileClipboardContentType::Text;
	Request.bCallerConfirmsUserInitiated = true;
	PasteAction = UOpenMobileDeviceUserInitiatedPasteAsyncAction::RequestUserInitiatedPaste(
		this,
		Request
	);
	PasteAction->Success.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandlePasteSucceeded);
	PasteAction->Cancelled.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandlePasteCancelled);
	PasteAction->Failed.AddUniqueDynamic(this, &UOpenMobileDeviceDemoWidget::HandlePasteFailed);
	SetActionStatus(TEXT("Waiting for the user-initiated paste result..."), OpenMobileDeviceDemoPrivate::MutedColor);
	PasteAction->Activate();
}

void UOpenMobileDeviceDemoWidget::HandleBrightnessClicked()
{
	if (BrightnessHandle)
	{
		BrightnessHandle->Release();
		BrightnessHandle = nullptr;
		SetActionStatus(TEXT("Brightness override released."), OpenMobileDeviceDemoPrivate::MutedColor);
		RefreshSnapshots();
		return;
	}

	FOpenMobileBrightnessRequest Request;
	Request.Brightness = 0.8f;
	BrightnessHandle = DeviceSubsystem->RequestBrightnessOverride(Request);
	const bool bActive = BrightnessHandle && BrightnessHandle->IsActive();
	SetActionStatus(
		bActive ? TEXT("Brightness override retained at 80%.") : TEXT("Brightness override was not accepted."),
		bActive ? OpenMobileDeviceDemoPrivate::SuccessColor : OpenMobileDeviceDemoPrivate::WarningColor
	);
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleAwakeClicked()
{
	if (AwakeHandle)
	{
		AwakeHandle->Release();
		AwakeHandle = nullptr;
		SetActionStatus(TEXT("Keep-awake handle released."), OpenMobileDeviceDemoPrivate::MutedColor);
		RefreshSnapshots();
		return;
	}

	AwakeHandle = DeviceSubsystem->RequestKeepScreenAwake();
	const bool bActive = AwakeHandle && AwakeHandle->IsActive();
	SetActionStatus(
		bActive ? TEXT("Keep-awake handle retained.") : TEXT("Keep-awake request was not accepted."),
		bActive ? OpenMobileDeviceDemoPrivate::SuccessColor : OpenMobileDeviceDemoPrivate::WarningColor
	);
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleSettingsClicked()
{
	const FOpenMobileApplicationSettingsOpenResult Result = DeviceSubsystem->OpenApplicationSettings();
	SetActionStatus(
		Result.IsAccepted()
			? TEXT("Application settings accepted. Capabilities refresh when the app returns.")
			: FString::Printf(
				TEXT("Application settings result: %s"),
				*OpenMobileDeviceDemoPrivate::EnumName(Result.State)
			),
		Result.IsAccepted()
			? OpenMobileDeviceDemoPrivate::SuccessColor
			: OpenMobileDeviceDemoPrivate::WarningColor
	);
}

void UOpenMobileDeviceDemoWidget::HandlePowerChanged(const FOpenMobilePowerSnapshot& Snapshot)
{
	static_cast<void>(Snapshot);
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleMemoryChanged(const FOpenMobileMemorySnapshot& Snapshot)
{
	static_cast<void>(Snapshot);
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleStorageChanged(const FOpenMobileStorageSnapshot& Snapshot)
{
	static_cast<void>(Snapshot);
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleNetworkChanged(
	const FOpenMobileNetworkPathSnapshot& Snapshot
)
{
	if (bHasPreviousNetworkSnapshot
		&& UOpenMobileDeviceBlueprintExamples::IsNetworkHandoff(
			PreviousNetworkSnapshot,
			Snapshot
		))
	{
		SetActionStatus(TEXT("Live event: network handoff detected."), OpenMobileDeviceDemoPrivate::WarningColor);
	}
	PreviousNetworkSnapshot = Snapshot;
	bHasPreviousNetworkSnapshot = true;
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleWindowChanged(
	const FOpenMobileWindowDisplaySnapshot& Snapshot
)
{
	if (bHasPreviousWindowSnapshot
		&& UOpenMobileDeviceBlueprintExamples::ShouldRebuildSafeArea(
			PreviousWindowSnapshot,
			Snapshot
		))
	{
		SetActionStatus(TEXT("Live event: rebuilding the game-owned safe-area layout."), OpenMobileDeviceDemoPrivate::WarningColor);
	}
	PreviousWindowSnapshot = Snapshot;
	bHasPreviousWindowSnapshot = true;
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleAppearanceChanged(
	const FOpenMobileAppearanceSnapshot& Snapshot
)
{
	static_cast<void>(Snapshot);
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleAccessibilityChanged(
	const FOpenMobileAccessibilitySnapshot& Snapshot
)
{
	static_cast<void>(Snapshot);
	RefreshSnapshots();
}

void UOpenMobileDeviceDemoWidget::HandleSettingsReturned(
	const FOpenMobileDeviceCapabilityReport& Report
)
{
	UpdateCapabilityReport(Report);
	SetActionStatus(TEXT("Returned from settings. Capability reasons refreshed."), OpenMobileDeviceDemoPrivate::SuccessColor);
}

void UOpenMobileDeviceDemoWidget::HandleReachabilityCompleted(
	const FOpenMobileEndpointReachabilityResult& Result
)
{
	SetActionStatus(
		FString::Printf(
			TEXT("Reachability: %s | HTTP %d | %.2f seconds"),
			*OpenMobileDeviceDemoPrivate::EnumName(Result.Outcome),
			Result.bHttpStatusAvailable ? Result.HttpStatusCode : 0,
			Result.DurationSeconds
		),
		Result.IsReachable()
			? OpenMobileDeviceDemoPrivate::SuccessColor
			: OpenMobileDeviceDemoPrivate::WarningColor
	);
	ReachabilityAction = nullptr;
}

void UOpenMobileDeviceDemoWidget::HandleReachabilityCancelled(const FOpenMobileError& Error)
{
	SetActionStatus(
		FString::Printf(TEXT("Reachability cancelled: %s"), *Error.Message),
		OpenMobileDeviceDemoPrivate::WarningColor
	);
	ReachabilityAction = nullptr;
}

void UOpenMobileDeviceDemoWidget::HandleReachabilityFailed(const FOpenMobileError& Error)
{
	SetActionStatus(
		FString::Printf(TEXT("Reachability failed: %s"), *Error.Message),
		OpenMobileDeviceDemoPrivate::ErrorColor
	);
	ReachabilityAction = nullptr;
}

void UOpenMobileDeviceDemoWidget::HandlePasteSucceeded()
{
	const int32 CharacterCount = PasteAction && PasteAction->Result.Content.Text.bIsAvailable
		? PasteAction->Result.Content.Text.Value.Len()
		: 0;
	SetActionStatus(
		FString::Printf(TEXT("Paste succeeded with %d text characters."), CharacterCount),
		OpenMobileDeviceDemoPrivate::SuccessColor
	);
	PasteAction = nullptr;
}

void UOpenMobileDeviceDemoWidget::HandlePasteCancelled(const FOpenMobileError& Error)
{
	SetActionStatus(
		FString::Printf(TEXT("Paste cancelled: %s"), *Error.Message),
		OpenMobileDeviceDemoPrivate::WarningColor
	);
	PasteAction = nullptr;
}

void UOpenMobileDeviceDemoWidget::HandlePasteFailed(const FOpenMobileError& Error)
{
	SetActionStatus(
		FString::Printf(TEXT("Paste failed: %s"), *Error.Message),
		OpenMobileDeviceDemoPrivate::ErrorColor
	);
	PasteAction = nullptr;
}

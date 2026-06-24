#include "OpenMobileDeviceDiagnosticsScreen.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "IDesktopPlatform.h"
#include "OpenMobileDeviceDiagnosticsOutput.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace OpenMobileDeviceDiagnosticsScreenPrivate
{
	const FName TabName(TEXT("OpenMobileDeviceDiagnostics"));

	class SOpenMobileDeviceDiagnostics final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SOpenMobileDeviceDiagnostics) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& Arguments)
		{
			static_cast<void>(Arguments);
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Refresh")))
						.OnClicked(this, &SOpenMobileDeviceDiagnostics::Refresh)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Copy")))
						.OnClicked(this, &SOpenMobileDeviceDiagnostics::Copy)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Export")))
						.OnClicked(this, &SOpenMobileDeviceDiagnostics::Export)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SAssignNew(StatusText, STextBlock)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(8.0f, 0.0f, 8.0f, 8.0f)
				[
					SAssignNew(OutputText, SMultiLineEditableTextBox)
					.IsReadOnly(true)
				]
			];
			Refresh();
		}

	private:
		FReply Refresh()
		{
			Snapshot = FOpenMobileDeviceDiagnosticsOutput::Capture();
			FString Json;
			const FOpenMobileDeviceDiagnosticsOutputResult Result =
				FOpenMobileDeviceDiagnosticsOutput::Serialize(Snapshot, Json);
			OutputText->SetText(FText::FromString(Json));
			SetStatus(Result, TEXT("Refreshed"));
			return FReply::Handled();
		}

		FReply Copy()
		{
			const FOpenMobileDeviceDiagnosticsOutputResult Result =
				FOpenMobileDeviceDiagnosticsOutput::CopyToClipboard(Snapshot);
			SetStatus(Result, TEXT("Copied"));
			return FReply::Handled();
		}

		FReply Export()
		{
			IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
			if (!DesktopPlatform)
			{
				StatusText->SetText(FText::FromString(
					TEXT("File dialog unavailable")
				));
				return FReply::Handled();
			}
			TArray<FString> FileNames;
			const void* ParentWindow =
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(
					nullptr
				);
			if (!DesktopPlatform->SaveFileDialog(
				ParentWindow,
				TEXT("Export OpenMobile Device Diagnostics"),
				FString(),
				TEXT("openmobile-device-diagnostics.json"),
				TEXT("JSON files (*.json)|*.json"),
				EFileDialogFlags::None,
				FileNames
			) || FileNames.IsEmpty())
			{
				StatusText->SetText(FText::FromString(TEXT("Export cancelled")));
				return FReply::Handled();
			}
			const FOpenMobileDeviceDiagnosticsOutputResult Result =
				FOpenMobileDeviceDiagnosticsOutput::ExportToFile(
					Snapshot,
					FileNames[0]
				);
			SetStatus(Result, TEXT("Exported"));
			return FReply::Handled();
		}

		void SetStatus(
			const FOpenMobileDeviceDiagnosticsOutputResult& Result,
			const TCHAR* SuccessText
		)
		{
			StatusText->SetText(FText::FromString(
				Result.IsSuccess() ? SuccessText : Result.Message
			));
		}

		FOpenMobileDeviceDiagnosticsSnapshot Snapshot;
		TSharedPtr<SMultiLineEditableTextBox> OutputText;
		TSharedPtr<STextBlock> StatusText;
	};
}

void FOpenMobileDeviceDiagnosticsScreen::Register()
{
	using namespace OpenMobileDeviceDiagnosticsScreenPrivate;
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabName,
		FOnSpawnTab::CreateStatic(&FOpenMobileDeviceDiagnosticsScreen::Spawn)
	)
	.SetDisplayName(FText::FromString(TEXT("OpenMobile Device Diagnostics")));
}

void FOpenMobileDeviceDiagnosticsScreen::Unregister()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(
		OpenMobileDeviceDiagnosticsScreenPrivate::TabName
	);
}

TSharedRef<SDockTab> FOpenMobileDeviceDiagnosticsScreen::Spawn(
	const FSpawnTabArgs& Args
)
{
	static_cast<void>(Args);
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(OpenMobileDeviceDiagnosticsScreenPrivate::SOpenMobileDeviceDiagnostics)
		];
}

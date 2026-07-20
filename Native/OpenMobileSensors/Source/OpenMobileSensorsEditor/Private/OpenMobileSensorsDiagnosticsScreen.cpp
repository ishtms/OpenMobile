#include "OpenMobileSensorsDiagnosticsScreen.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformTime.h"
#include "IDesktopPlatform.h"
#include "OpenMobileSensorsDiagnosticsOutput.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace OpenMobileSensorsDiagnosticsScreenPrivate
{
	const FName TabName(TEXT("OpenMobileSensorsDiagnostics"));
	constexpr double RefreshIntervalSeconds = 0.5;

	class SOpenMobileSensorsDiagnostics final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SOpenMobileSensorsDiagnostics) {}
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
						.OnClicked(this, &SOpenMobileSensorsDiagnostics::Refresh)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Copy redacted JSON")))
						.OnClicked(this, &SOpenMobileSensorsDiagnostics::Copy)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Export redacted JSON")))
						.OnClicked(this, &SOpenMobileSensorsDiagnostics::Export)
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
			RefreshSnapshot();
			NextRefreshSeconds =
				FPlatformTime::Seconds() + RefreshIntervalSeconds;
		}

		virtual void Tick(
			const FGeometry& AllottedGeometry,
			const double InCurrentTime,
			const float InDeltaTime
		) override
		{
			SCompoundWidget::Tick(
				AllottedGeometry,
				InCurrentTime,
				InDeltaTime
			);
			if (InCurrentTime >= NextRefreshSeconds)
			{
				RefreshSnapshot();
				NextRefreshSeconds =
					InCurrentTime + RefreshIntervalSeconds;
			}
		}

	private:
		void RefreshSnapshot()
		{
			Snapshot = FOpenMobileSensorsDiagnosticsOutput::Capture();
			FString Json;
			const FOpenMobileSensorsDiagnosticsOutputResult Result =
				FOpenMobileSensorsDiagnosticsOutput::Serialize(Snapshot, Json);
			OutputText->SetText(FText::FromString(
				Result.IsSuccess() ? Json : Result.Message
			));
			StatusText->SetText(FText::FromString(
				Result.IsSuccess()
					? FString::Printf(
						TEXT("%d logical, %d physical, %lld dropped"),
						Snapshot.Streams.Num(),
						Snapshot.PhysicalStreams.Num(),
						CountDroppedSamples()
					)
					: Result.Message
			));
		}

		int64 CountDroppedSamples() const
		{
			int64 Total = 0;
			for (const FOpenMobileSensorStreamDiagnostics& Stream
				: Snapshot.Streams)
			{
				Total += Stream.DroppedSamples;
			}
			return Total;
		}

		FReply Refresh()
		{
			RefreshSnapshot();
			return FReply::Handled();
		}

		FReply Copy()
		{
			const FOpenMobileSensorsDiagnosticsOutputResult Result =
				FOpenMobileSensorsDiagnosticsOutput::CopyToClipboard(Snapshot);
			StatusText->SetText(FText::FromString(
				Result.IsSuccess() ? TEXT("Copied") : Result.Message
			));
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
				TEXT("Export OpenMobile Sensor Diagnostics"),
				FString(),
				TEXT("openmobile-sensor-diagnostics.json"),
				TEXT("JSON files (*.json)|*.json"),
				EFileDialogFlags::None,
				FileNames
			) || FileNames.IsEmpty())
			{
				StatusText->SetText(FText::FromString(TEXT("Export cancelled")));
				return FReply::Handled();
			}
			const FOpenMobileSensorsDiagnosticsOutputResult Result =
				FOpenMobileSensorsDiagnosticsOutput::ExportToFile(
					Snapshot,
					FileNames[0]
				);
			StatusText->SetText(FText::FromString(
				Result.IsSuccess() ? TEXT("Exported") : Result.Message
			));
			return FReply::Handled();
		}

		FOpenMobileSensorDiagnosticsSnapshot Snapshot;
		TSharedPtr<SMultiLineEditableTextBox> OutputText;
		TSharedPtr<STextBlock> StatusText;
		double NextRefreshSeconds = 0.0;
	};
}

void FOpenMobileSensorsDiagnosticsScreen::Register()
{
	using namespace OpenMobileSensorsDiagnosticsScreenPrivate;
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabName,
		FOnSpawnTab::CreateStatic(&FOpenMobileSensorsDiagnosticsScreen::Spawn)
	)
	.SetDisplayName(FText::FromString(TEXT("OpenMobile Sensor Diagnostics")));
}

void FOpenMobileSensorsDiagnosticsScreen::Unregister()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(
		OpenMobileSensorsDiagnosticsScreenPrivate::TabName
	);
}

TSharedRef<SDockTab> FOpenMobileSensorsDiagnosticsScreen::Spawn(
	const FSpawnTabArgs& Args
)
{
	static_cast<void>(Args);
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(
				OpenMobileSensorsDiagnosticsScreenPrivate::
				SOpenMobileSensorsDiagnostics
			)
		];
}

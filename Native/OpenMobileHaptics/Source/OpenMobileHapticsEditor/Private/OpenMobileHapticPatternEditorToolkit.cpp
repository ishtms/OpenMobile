#include "OpenMobileHapticPatternEditorToolkit.h"

#include "Editor.h"
#include "Framework/Commands/GenericCommands.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticTimelineEditorModel.h"
#include "PropertyEditorModule.h"
#include "SOpenMobileHapticTimelineEditor.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "OpenMobileHapticPatternEditorToolkit"

namespace OpenMobileHapticPatternEditorToolkitPrivate
{
	const FName AppIdentifier(TEXT("OpenMobileHapticPatternEditor"));
	const FName TimelineTabId(TEXT("OpenMobileHapticPatternEditor.Timeline"));
	const FName DetailsTabId(TEXT("OpenMobileHapticPatternEditor.Details"));
}

FOpenMobileHapticPatternEditorToolkit::~FOpenMobileHapticPatternEditorToolkit()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void FOpenMobileHapticPatternEditorToolkit::Initialize(
	UOpenMobileHapticPatternAsset* InAsset,
	EToolkitMode::Type InMode,
	const TSharedPtr<IToolkitHost>& InToolkitHost
)
{
	check(InAsset);
	Asset = InAsset;
	InAsset->SetFlags(RF_Transactional);
	Model = MakeShared<FOpenMobileHapticTimelineEditorModel>(InAsset);

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NotifyHook = nullptr;
	FPropertyEditorModule& PropertyEditor =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(
			TEXT("PropertyEditor")
		);
	DetailsView = PropertyEditor.CreateDetailView(DetailsArgs);
	DetailsView->SetObject(InAsset);
	DetailsView->OnFinishedChangingProperties().AddSP(
		this,
		&FOpenMobileHapticPatternEditorToolkit::HandleDetailsChanged
	);

	BindCommands();
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}

	using namespace OpenMobileHapticPatternEditorToolkitPrivate;
	const TSharedRef<FTabManager::FLayout> Layout =
		FTabManager::NewLayout(TEXT("OpenMobileHapticPatternEditor_Layout_v1"))
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(
				FTabManager::NewStack()
				->AddTab(TimelineTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
				->SetSizeCoefficient(0.70f)
			)
			->Split(
				FTabManager::NewStack()
				->AddTab(DetailsTabId, ETabState::OpenedTab)
				->SetSizeCoefficient(0.30f)
			)
		);

	InitAssetEditor(
		InMode,
		InToolkitHost,
		AppIdentifier,
		Layout,
		true,
		true,
		InAsset
	);
	RegenerateMenusAndToolbars();
}

void FOpenMobileHapticPatternEditorToolkit::RegisterTabSpawners(
	const TSharedRef<FTabManager>& InTabManager
)
{
	using namespace OpenMobileHapticPatternEditorToolkitPrivate;
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceCategory", "OpenMobile Haptic Pattern")
	);
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	InTabManager->RegisterTabSpawner(
		TimelineTabId,
		FOnSpawnTab::CreateSP(
			this,
			&FOpenMobileHapticPatternEditorToolkit::SpawnTimelineTab
		)
	)
	.SetDisplayName(LOCTEXT("TimelineTab", "Timeline"))
	.SetGroup(WorkspaceMenuCategory.ToSharedRef())
	.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Timeline")));
	InTabManager->RegisterTabSpawner(
		DetailsTabId,
		FOnSpawnTab::CreateSP(
			this,
			&FOpenMobileHapticPatternEditorToolkit::SpawnDetailsTab
		)
	)
	.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
	.SetGroup(WorkspaceMenuCategory.ToSharedRef())
	.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Details")));
}

void FOpenMobileHapticPatternEditorToolkit::UnregisterTabSpawners(
	const TSharedRef<FTabManager>& InTabManager
)
{
	using namespace OpenMobileHapticPatternEditorToolkitPrivate;
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(TimelineTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

FName FOpenMobileHapticPatternEditorToolkit::GetToolkitFName() const
{
	return TEXT("OpenMobileHapticPatternEditor");
}

FText FOpenMobileHapticPatternEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "OpenMobile Haptic Pattern");
}

FString FOpenMobileHapticPatternEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("Haptic Pattern ");
}

FLinearColor
FOpenMobileHapticPatternEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.16f, 0.64f, 0.88f);
}

bool FOpenMobileHapticPatternEditorToolkit::CanSaveAsset() const
{
	const UOpenMobileHapticPatternAsset* PatternAsset = Asset.Get();
	TArray<FString> Errors;
	return PatternAsset && PatternAsset->ValidateForEditor(Errors);
}

void FOpenMobileHapticPatternEditorToolkit::PostUndo(bool bSuccess)
{
	if (bSuccess && Model)
	{
		Model->RefreshAfterExternalChange();
	}
}

void FOpenMobileHapticPatternEditorToolkit::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

TSharedRef<SDockTab>
FOpenMobileHapticPatternEditorToolkit::SpawnTimelineTab(
	const FSpawnTabArgs& Args
)
{
	static_cast<void>(Args);
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SOpenMobileHapticTimelineEditor)
			.Model(Model)
		];
}

TSharedRef<SDockTab>
FOpenMobileHapticPatternEditorToolkit::SpawnDetailsTab(
	const FSpawnTabArgs& Args
)
{
	static_cast<void>(Args);
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			DetailsView.ToSharedRef()
		];
}

void FOpenMobileHapticPatternEditorToolkit::BindCommands()
{
	const TWeakPtr<FOpenMobileHapticTimelineEditorModel> WeakModel = Model;
	ToolkitCommands->MapAction(
		FGenericCommands::Get().Undo,
		FExecuteAction::CreateLambda([]
		{
			if (GEditor)
			{
				GEditor->UndoTransaction();
			}
		})
	);
	ToolkitCommands->MapAction(
		FGenericCommands::Get().Redo,
		FExecuteAction::CreateLambda([]
		{
			if (GEditor)
			{
				GEditor->RedoTransaction();
			}
		})
	);
	ToolkitCommands->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateLambda([WeakModel]
		{
			if (const TSharedPtr<FOpenMobileHapticTimelineEditorModel> Pinned =
				WeakModel.Pin())
			{
				Pinned->CopySelection();
			}
		}),
		FCanExecuteAction::CreateLambda([WeakModel]
		{
			const TSharedPtr<FOpenMobileHapticTimelineEditorModel> Pinned =
				WeakModel.Pin();
			return Pinned && Pinned->CanCopy();
		})
	);
	ToolkitCommands->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateLambda([WeakModel]
		{
			if (const TSharedPtr<FOpenMobileHapticTimelineEditorModel> Pinned =
				WeakModel.Pin())
			{
				Pinned->PasteAtCursor();
			}
		}),
		FCanExecuteAction::CreateLambda([WeakModel]
		{
			const TSharedPtr<FOpenMobileHapticTimelineEditorModel> Pinned =
				WeakModel.Pin();
			return Pinned && Pinned->CanPaste();
		})
	);
	ToolkitCommands->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateLambda([WeakModel]
		{
			if (const TSharedPtr<FOpenMobileHapticTimelineEditorModel> Pinned =
				WeakModel.Pin())
			{
				Pinned->DeleteSelection();
			}
		}),
		FCanExecuteAction::CreateLambda([WeakModel]
		{
			const TSharedPtr<FOpenMobileHapticTimelineEditorModel> Pinned =
				WeakModel.Pin();
			return Pinned && Pinned->CanCopy();
		})
	);
	ToolkitCommands->MapAction(
		FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateLambda([WeakModel]
		{
			if (const TSharedPtr<FOpenMobileHapticTimelineEditorModel> Pinned =
				WeakModel.Pin())
			{
				Pinned->SelectAllEvents();
			}
		})
	);
}

void FOpenMobileHapticPatternEditorToolkit::HandleDetailsChanged(
	const FPropertyChangedEvent& Event
)
{
	static_cast<void>(Event);
	if (Model)
	{
		Model->RefreshAfterExternalChange();
	}
}

#undef LOCTEXT_NAMESPACE

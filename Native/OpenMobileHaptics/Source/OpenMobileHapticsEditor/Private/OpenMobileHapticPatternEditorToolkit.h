#pragma once

#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"

class FOpenMobileHapticTimelineEditorModel;
class IDetailsView;
class SDockTab;
class UOpenMobileHapticPatternAsset;
struct FPropertyChangedEvent;

class FOpenMobileHapticPatternEditorToolkit final
	: public FAssetEditorToolkit
	, public FEditorUndoClient
{
public:
	~FOpenMobileHapticPatternEditorToolkit() override;

	void Initialize(
		UOpenMobileHapticPatternAsset* InAsset,
		EToolkitMode::Type InMode,
		const TSharedPtr<IToolkitHost>& InToolkitHost
	);

	virtual void RegisterTabSpawners(
		const TSharedRef<FTabManager>& InTabManager
	) override;
	virtual void UnregisterTabSpawners(
		const TSharedRef<FTabManager>& InTabManager
	) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual bool CanSaveAsset() const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	TSharedRef<SDockTab> SpawnTimelineTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	void BindCommands();
	void HandleDetailsChanged(const FPropertyChangedEvent& Event);

	TWeakObjectPtr<UOpenMobileHapticPatternAsset> Asset;
	TSharedPtr<FOpenMobileHapticTimelineEditorModel> Model;
	TSharedPtr<IDetailsView> DetailsView;
};

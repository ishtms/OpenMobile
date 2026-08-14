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
	/** Unregisters editor callbacks before weak asset and model state disappear. */
	~FOpenMobileHapticPatternEditorToolkit() override;

	/** Builds one timeline editor around the supplied asset and registers undo against the active editor context. */
	void Initialize(
		UOpenMobileHapticPatternAsset* InAsset,
		EToolkitMode::Type InMode,
		const TSharedPtr<IToolkitHost>& InToolkitHost
	);

	/** Adds timeline and details tabs to this toolkit's workspace only. */
	virtual void RegisterTabSpawners(
		const TSharedRef<FTabManager>& InTabManager
	) override;
	/** Removes tab factories before the toolkit manager releases this editor instance. */
	virtual void UnregisterTabSpawners(
		const TSharedRef<FTabManager>& InTabManager
	) override;
	/** Returns a stable toolkit id so layouts restore to the right asset editor. */
	virtual FName GetToolkitFName() const override;
	/** Supplies the user-facing editor name independently of the currently opened asset. */
	virtual FText GetBaseToolkitName() const override;
	/** Keeps world-centric tab labels recognisable when several asset editors share one host. */
	virtual FString GetWorldCentricTabPrefix() const override;
	/** Applies the Haptics editor tint to world-centric tabs. */
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	/** Prevents saving while the timeline model reports data the runtime compiler would reject. */
	virtual bool CanSaveAsset() const override;
	/** Rebuilds derived timeline state after a successful editor undo. */
	virtual void PostUndo(bool bSuccess) override;
	/** Rebuilds the same derived state after redo so Slate doesn't retain stale rows. */
	virtual void PostRedo(bool bSuccess) override;

private:
	/** Creates the timeline tab lazily because restored layouts may never open it. */
	TSharedRef<SDockTab> SpawnTimelineTab(const FSpawnTabArgs& Args);
	/** Creates the details panel lazily and binds it to the editor's current asset. */
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	/** Connects editor commands to model mutations with their availability checks. */
	void BindCommands();
	/** Refreshes compiled validation when details editing bypasses timeline commands. */
	void HandleDetailsChanged(const FPropertyChangedEvent& Event);

	TWeakObjectPtr<UOpenMobileHapticPatternAsset> Asset;
	TSharedPtr<FOpenMobileHapticTimelineEditorModel> Model;
	TSharedPtr<IDetailsView> DetailsView;
};

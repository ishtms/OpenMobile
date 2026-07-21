#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "OpenMobileHapticAHAPFactory.h"
#include "OpenMobileHapticPatternFactory.h"
#include "OpenMobileHapticTimelineEditorModel.h"

#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticTimelineEditorTest,
	"OpenMobile.Haptics.Editor.TimelineWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticTimelineEditorTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString PackageName = TEXT("/Game/OpenMobileHapticsTests/Timeline_")
		+ Suffix;
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetAssetPackageExtension()
	);
	TestTrue(TEXT("Package directory is available"),
		IFileManager::Get().MakeDirectory(
			*FPaths::GetPath(PackageFilename),
			true
		));
	UPackage* Package = CreatePackage(*PackageName);
	UOpenMobileHapticPatternFactory* PatternFactory =
		NewObject<UOpenMobileHapticPatternFactory>();
	UOpenMobileHapticPatternAsset* Asset =
		Cast<UOpenMobileHapticPatternAsset>(PatternFactory->FactoryCreateNew(
			UOpenMobileHapticPatternAsset::StaticClass(),
			Package,
			TEXT("Timeline"),
			RF_Public | RF_Standalone | RF_Transactional,
			nullptr,
			GWarn
		));
	TestNotNull(TEXT("Factory creates a pattern asset"), Asset);
	if (!Asset)
	{
		return false;
	}

	TSharedPtr<FOpenMobileHapticTimelineEditorModel> Model =
		MakeShared<FOpenMobileHapticTimelineEditorModel>(Asset);
	Model->SelectEvent(0, false, false);
	TestTrue(TEXT("Event type edits"), Model->SetSelectedEventType(
		EOpenMobileHapticPatternEventType::Continuous));
	TestTrue(TEXT("Event duration edits"),
		Model->SetSelectedEventDuration(0.1));
	TestTrue(TEXT("Event intensity edits"),
		Model->SetSelectedEventIntensity(0.6f));
	TestTrue(TEXT("Event sharpness edits"),
		Model->SetSelectedEventSharpness(0.7f));
	TestTrue(TEXT("Event frequency intent edits"),
		Model->SetSelectedEventFrequencyIntent(0.4f));
	Model->SetCursorTimeSeconds(0.2);
	TestTrue(TEXT("A second event is added"), Model->AddEvent(
		EOpenMobileHapticPatternEventType::Transient));
	Model->SelectEvent(0, false, false);
	Model->SelectEvent(1, true, false);
	TestTrue(TEXT("Multi-selection edits both events"),
		Model->SetSelectedEventIntensity(0.75f));

	FString PreviousClipboard;
	FPlatformApplicationMisc::ClipboardPaste(PreviousClipboard);
	Model->CopySelection();
	Model->SetCursorTimeSeconds(0.5);
	TestTrue(TEXT("Copied events paste at the cursor"),
		Model->PasteAtCursor());
	TestEqual(TEXT("Paste adds both selected events"),
		Asset->SourcePattern.Events.Num(), 4);
	FPlatformApplicationMisc::ClipboardCopy(*PreviousClipboard);

	Model->SetCursorTimeSeconds(0.85);
	TestTrue(TEXT("Marker is added"), Model->AddMarker());
	TestTrue(TEXT("Marker is renamed"),
		Model->SetSelectedMarkerName(TEXT("Impact")));
	TestTrue(TEXT("Category edits"),
		Model->SetDefaultCategory(TEXT("Combat")));
	TestTrue(TEXT("Loop edits"), Model->SetLoopEnabled(true));
	TestTrue(TEXT("Loop edit is applied"), Asset->Loop.bLoop);
	TestTrue(TEXT("Undo is available"), GEditor != nullptr);
	if (GEditor)
	{
		GEditor->UndoTransaction();
		TestFalse(TEXT("Undo restores loop state"), Asset->Loop.bLoop);
		GEditor->RedoTransaction();
		TestTrue(TEXT("Redo restores loop state"), Asset->Loop.bLoop);
	}

	FOpenMobileHapticParameterCurve Curve;
	Curve.Parameter = EOpenMobileHapticCurveParameter::IntensityControl;
	Curve.StartTimeSeconds = 0.0;
	FOpenMobileHapticCurvePoint& LaterPoint =
		Curve.ControlPoints.AddDefaulted_GetRef();
	LaterPoint.RelativeTimeSeconds = 0.1;
	LaterPoint.Value = 0.8f;
	FOpenMobileHapticCurvePoint& FirstPoint =
		Curve.ControlPoints.AddDefaulted_GetRef();
	FirstPoint.RelativeTimeSeconds = 0.0;
	FirstPoint.Value = 0.2f;
	Asset->SourcePattern.ParameterCurves.Add(Curve);
	Asset->NormalizeEditorData();
	TArray<FString> Errors;
	TestTrue(TEXT("Valid control points compile"),
		Asset->RebuildDerivedData(Errors));
	Asset->SourcePattern.ParameterCurves[0].ControlPoints[0].Value = 2.0f;
	TestFalse(TEXT("Invalid control points block validation"),
		Asset->ValidateForEditor(Errors));
	Asset->SourcePattern.ParameterCurves[0].ControlPoints[0].Value = 0.2f;
	Asset->NormalizeEditorData();
	TestTrue(TEXT("Repaired control points compile"),
		Asset->RebuildDerivedData(Errors));

	const FString AHAPFilename = FPaths::CreateTempFilename(
		FPlatformProcess::UserTempDir(),
		TEXT("OpenMobileTimeline"),
		TEXT(".ahap")
	);
	const FString AHAPSource = TEXT(
		"{\"Version\":1,\"Pattern\":[{\"Event\":{"
		"\"EventType\":\"HapticTransient\",\"Time\":0}}]}"
	);
	TestTrue(TEXT("AHAP fixture is written"),
		FFileHelper::SaveStringToFile(AHAPSource, *AHAPFilename));
	UOpenMobileHapticAHAPFactory* AHAPFactory =
		NewObject<UOpenMobileHapticAHAPFactory>();
	bool bCancelled = true;
	UOpenMobileHapticIOSPatternAsset* IOSOverride =
		Cast<UOpenMobileHapticIOSPatternAsset>(AHAPFactory->FactoryCreateFile(
			UOpenMobileHapticIOSPatternAsset::StaticClass(),
			Package,
			TEXT("TimelineIOS"),
			RF_Public | RF_Standalone | RF_Transactional,
			AHAPFilename,
			nullptr,
			GWarn,
			bCancelled
		));
	TestNotNull(TEXT("AHAP override imports"), IOSOverride);
	if (IOSOverride)
	{
		Asset->IOSOverride = IOSOverride;
		TestTrue(TEXT("iOS override passes its cook filter"),
			IOSOverride->ShouldCookForPlatform(TEXT("IOS")));
		TestFalse(TEXT("iOS override is excluded from Android cook"),
			IOSOverride->ShouldCookForPlatform(TEXT("Android")));
	}
	TestTrue(TEXT("Edited asset validates before save and cook"),
		Asset->ValidateForEditor(Errors));

	FOpenMobileHapticCookedPatternData BeforeSave = Asset->GetCookedPattern();
	TArray<uint8> BeforeSaveBytes;
	FMemoryWriter BeforeWriter(BeforeSaveBytes);
	BeforeSave.Serialize(BeforeWriter);
	BeforeWriter.Close();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	TestTrue(TEXT("Asset package saves"), UPackage::SavePackage(
		Package,
		Asset,
		*PackageFilename,
		SaveArgs
	));
	Model.Reset();
	Asset = nullptr;
	IOSOverride = nullptr;
	const FName DiscardedName = MakeUniqueObjectName(
		GetTransientPackage(),
		UPackage::StaticClass(),
		TEXT("DiscardedHapticTimeline")
	);
	Package->Rename(
		*DiscardedName.ToString(),
		GetTransientPackage(),
		REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty
	);
	Package = nullptr;

	UPackage* LoadedPackage = LoadPackage(nullptr, *PackageName, LOAD_None);
	TestNotNull(TEXT("Saved package reloads"), LoadedPackage);
	UOpenMobileHapticPatternAsset* Reloaded = LoadedPackage
		? FindObject<UOpenMobileHapticPatternAsset>(LoadedPackage, TEXT("Timeline"))
		: nullptr;
	TestNotNull(TEXT("Pattern asset reloads"), Reloaded);
	if (Reloaded)
	{
		TestEqual(TEXT("Category survives reload"),
			Reloaded->DefaultCategory, FName(TEXT("Combat")));
		TestEqual(TEXT("Events survive reload"),
			Reloaded->SourcePattern.Events.Num(), 4);
		TestEqual(TEXT("Markers survive reload"), Reloaded->Markers.Num(), 1);
		TestTrue(TEXT("Reloaded asset validates"),
			Reloaded->ValidateForEditor(Errors));
		FOpenMobileHapticCookedPatternData AfterLoad =
			Reloaded->GetCookedPattern();
		TArray<uint8> AfterLoadBytes;
		FMemoryWriter AfterWriter(AfterLoadBytes);
		AfterLoad.Serialize(AfterWriter);
		AfterWriter.Close();
		TestTrue(TEXT("Cooked serialization is deterministic across reload"),
			BeforeSaveBytes == AfterLoadBytes);
	}

	IFileManager::Get().Delete(*AHAPFilename);
	IFileManager::Get().Delete(*PackageFilename);
	return true;
}

#endif

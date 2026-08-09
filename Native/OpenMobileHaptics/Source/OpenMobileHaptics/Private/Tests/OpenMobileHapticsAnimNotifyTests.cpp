#if WITH_DEV_AUTOMATION_TESTS

#include "AnimNotify_OpenMobileHaptics.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAnimNotifyTest,
	"OpenMobile.Haptics.Integration.AnimationNotify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAnimNotifyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileHapticsAnimNotifyTest"));
	UWorld* World = GameInstance->GetWorld();
	UOpenMobileHapticsSubsystem* Haptics =
		GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>();
	TestNotNull(TEXT("Standalone world owns the Haptics subsystem"), Haptics);
	Haptics->SetHapticsEnabled(false);

	UAnimNotify_OpenMobileHaptics* SemanticNotify =
		NewObject<UAnimNotify_OpenMobileHaptics>();
	SemanticNotify->Channel = TEXT("Animation");
	SemanticNotify->Category = TEXT("Combat");
	SemanticNotify->Priority = EOpenMobileHapticChannelPriority::High;
	USkeletalMeshComponent* FirstMesh =
		NewObject<USkeletalMeshComponent>(World);
	USkeletalMeshComponent* SecondMesh =
		NewObject<USkeletalMeshComponent>(World);
	int64 Dropped = Haptics->GetDiagnostics()
		.Performance.DroppedRequestCount;
	SemanticNotify->Dispatch(FirstMesh);
	SemanticNotify->Dispatch(FirstMesh);
	SemanticNotify->Dispatch(SecondMesh);
	TestEqual(TEXT("Repeated and independent mesh instances each submit"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 3);

	UAnimNotify_OpenMobileHaptics* NamedNotify =
		NewObject<UAnimNotify_OpenMobileHaptics>();
	NamedNotify->EffectMode =
		EOpenMobileHapticAnimNotifyEffectMode::NamedPattern;
	NamedNotify->NamedPattern = TEXT("Footstep.Stone");
	Dropped = Haptics->GetDiagnostics().Performance.DroppedRequestCount;
	NamedNotify->Dispatch(FirstMesh);
	TestEqual(TEXT("Named effects use the same owning subsystem"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 1);

	UAnimNotify_OpenMobileHaptics* GamePresetNotify =
		NewObject<UAnimNotify_OpenMobileHaptics>();
	GamePresetNotify->EffectMode =
		EOpenMobileHapticAnimNotifyEffectMode::GamePreset;
	GamePresetNotify->GamePreset = EOpenMobileHapticGamePreset::Damage;
	Dropped = Haptics->GetDiagnostics().Performance.DroppedRequestCount;
	GamePresetNotify->Dispatch(FirstMesh);
	TestEqual(TEXT("Game presets use the authored notify path"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 1);

	UOpenMobileHapticPatternAsset* Pattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	Pattern->SourcePattern.Events.AddDefaulted();
	TArray<FString> PatternErrors;
	TestTrue(TEXT("Notify test pattern builds"),
		Pattern->RebuildDerivedData(PatternErrors));
	UAnimNotify_OpenMobileHaptics* AssetNotify =
		NewObject<UAnimNotify_OpenMobileHaptics>();
	AssetNotify->EffectMode =
		EOpenMobileHapticAnimNotifyEffectMode::PatternAsset;
	AssetNotify->PatternAsset = Pattern;
	Dropped = Haptics->GetDiagnostics().Performance.DroppedRequestCount;
	AssetNotify->Dispatch(FirstMesh);
	TestEqual(TEXT("Pattern assets submit from the authored notify path"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 1);

#if WITH_EDITOR
	UAnimNotify_OpenMobileHaptics* InvalidNotify =
		NewObject<UAnimNotify_OpenMobileHaptics>();
	InvalidNotify->EffectMode =
		EOpenMobileHapticAnimNotifyEffectMode::PatternAsset;
	FDataValidationContext ValidationContext;
	TestEqual(TEXT("Missing pattern assets fail notify validation"),
		InvalidNotify->IsDataValid(ValidationContext),
		EDataValidationResult::Invalid);
#endif

	USkeletalMeshComponent* OrphanMesh =
		NewObject<USkeletalMeshComponent>();
	TestFalse(TEXT("Missing world is a safe no-op"),
		SemanticNotify->Dispatch(OrphanMesh));

	UWorld* PreviewWorld = UWorld::CreateWorld(
		EWorldType::EditorPreview,
		false,
		TEXT("OpenMobileHapticsAnimNotifyPreview")
	);
	USkeletalMeshComponent* PreviewMesh =
		NewObject<USkeletalMeshComponent>(PreviewWorld);
	TestFalse(TEXT("Editor preview never reaches Haptics"),
		SemanticNotify->Dispatch(PreviewMesh));
	PreviewWorld->DestroyWorld(false);

#if WITH_EDITOR
	UWorld* DedicatedWorld = UWorld::CreateWorld(
		EWorldType::PIE,
		false,
		TEXT("OpenMobileHapticsAnimNotifyDedicated")
	);
	DedicatedWorld->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
	USkeletalMeshComponent* DedicatedMesh =
		NewObject<USkeletalMeshComponent>(DedicatedWorld);
	TestEqual(TEXT("Dedicated test world reports server mode"),
		DedicatedWorld->GetNetMode(), NM_DedicatedServer);
	TestFalse(TEXT("Dedicated server suppression is enabled by default"),
		SemanticNotify->Dispatch(DedicatedMesh));
	DedicatedWorld->DestroyWorld(false);
#endif

	GameInstance->Shutdown();
	TestFalse(TEXT("Game Instance teardown leaves no dispatch target"),
		SemanticNotify->Dispatch(FirstMesh));
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

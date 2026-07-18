#if WITH_DEV_AUTOMATION_TESTS

#include "OpenMobileHapticsGameplayCueNotify.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsGameplayCueContractTest,
	"OpenMobile.Haptics.Integration.GameplayCue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsGameplayCueContractTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileHapticsGameplayCueTest"));
	UWorld* World = GameInstance->GetWorld();
	UOpenMobileHapticsSubsystem* Haptics =
		GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>();
	TestNotNull(TEXT("Standalone world owns the Haptics subsystem"), Haptics);
	Haptics->SetHapticsEnabled(false);

	APlayerController* FirstController =
		World->SpawnActor<APlayerController>();
	APlayerController* SecondController =
		World->SpawnActor<APlayerController>();
	FirstController->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	SecondController->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	TestTrue(TEXT("First cue target is locally controlled"),
		FirstController->IsLocalController());
	TestTrue(TEXT("Second cue target is locally controlled"),
		SecondController->IsLocalController());

	UOpenMobileHapticsGameplayCueNotify* Cue =
		GetMutableDefault<UOpenMobileHapticsGameplayCueNotify>();
	TestTrue(TEXT("Cue handles executed events"),
		Cue->HandlesEvent(EGameplayCueEvent::Executed));
	TestFalse(TEXT("Cue ignores active lifecycle events"),
		Cue->HandlesEvent(EGameplayCueEvent::OnActive));
	FGameplayCueParameters CueParameters;
	int64 Dropped = Haptics->GetDiagnostics()
		.Performance.DroppedRequestCount;
	Cue->HandleGameplayCue(
		FirstController,
		EGameplayCueEvent::Executed,
		CueParameters
	);
	Cue->HandleGameplayCue(
		FirstController,
		EGameplayCueEvent::Executed,
		CueParameters
	);
	Cue->HandleGameplayCue(
		SecondController,
		EGameplayCueEvent::Executed,
		CueParameters
	);
	TestEqual(TEXT("Each delivered event submits once for its local target"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 3);

	AActor* RemoteTarget = World->SpawnActor<AActor>();
	Dropped = Haptics->GetDiagnostics().Performance.DroppedRequestCount;
	TestFalse(TEXT("A target without local ownership is silent"),
		Cue->Submit(RemoteTarget, CueParameters));
	TestEqual(TEXT("Remote suppression does not reach the subsystem"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount, Dropped);

	Cue->EffectMode = EOpenMobileHapticsGameplayCueEffectMode::NamedPattern;
	Cue->NamedPattern = TEXT("Ability.Confirm");
	Cue->HandleGameplayCue(
		FirstController,
		EGameplayCueEvent::Executed,
		CueParameters
	);
	TestEqual(TEXT("Named effects use the same local delivery path"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 1);
	Cue->EffectMode = EOpenMobileHapticsGameplayCueEffectMode::Semantic;
	Cue->NamedPattern = NAME_None;

#if WITH_EDITOR
	UWorld* DedicatedWorld = UWorld::CreateWorld(
		EWorldType::PIE,
		false,
		TEXT("OpenMobileHapticsGameplayCueDedicated")
	);
	DedicatedWorld->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
	APlayerController* DedicatedController =
		DedicatedWorld->SpawnActor<APlayerController>();
	DedicatedController->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	TestFalse(TEXT("Dedicated servers are always silent"),
		Cue->Submit(DedicatedController, CueParameters));
	DedicatedWorld->DestroyWorld(false);
#endif

	GameInstance->Shutdown();
	TestFalse(TEXT("Game Instance teardown leaves no submission target"),
		Cue->Submit(FirstController, CueParameters));
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

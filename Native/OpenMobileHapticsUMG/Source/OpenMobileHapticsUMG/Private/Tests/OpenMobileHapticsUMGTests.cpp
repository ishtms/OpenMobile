#if WITH_DEV_AUTOMATION_TESTS

#include "OpenMobileHapticsWidgetComponent.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Input/NavigationMetadata.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsUMGContractTest,
	"OpenMobile.Haptics.UMG.InteractionComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsUMGContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileHapticsUMGTest"));
	UWorld* World = GameInstance->GetWorld();
	UOpenMobileHapticsSubsystem* Haptics =
		GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>();
	TestNotNull(TEXT("Standalone test owns the Haptics subsystem"), Haptics);
	Haptics->SetHapticsEnabled(false);
	TestFalse(TEXT("Disabled policy remains authoritative"),
		Haptics->IsHapticsEnabled());

	UButton* Button = NewObject<UButton>(World);
	UOpenMobileHapticsWidgetComponent* ButtonComponent =
		NewObject<UOpenMobileHapticsWidgetComponent>(Button);
	ButtonComponent->Initialize(Button);
	Button->TakeWidget();
	ButtonComponent->Construct();
	ButtonComponent->Construct();
	TestEqual(TEXT("Reconstruction keeps one button binding"),
		Button->OnPressed.GetAllObjects().FilterByPredicate(
			[ButtonComponent](const UObject* BoundObject)
			{
				return BoundObject == ButtonComponent;
			}
		).Num(), 1);
	TestEqual(TEXT("Reconstruction keeps one navigation hook"),
		Button->GetCachedWidget()
			->GetAllMetaData<FNavigationTransitionMetadata>().Num(), 1);
	int64 Dropped = Haptics->GetDiagnostics()
		.Performance.DroppedRequestCount;
	Button->OnPressed.Broadcast();
	TestEqual(TEXT("One press produces one suppressed submission"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 1);
	ButtonComponent->Destruct();
	TestEqual(TEXT("Destruct removes the button binding"),
		Button->OnPressed.GetAllObjects().FilterByPredicate(
			[ButtonComponent](const UObject* BoundObject)
			{
				return BoundObject == ButtonComponent;
			}
		).Num(), 0);
	TestEqual(TEXT("Destruct removes navigation metadata"),
		Button->GetCachedWidget()
			->GetAllMetaData<FNavigationTransitionMetadata>().Num(), 0);
	Dropped = Haptics->GetDiagnostics().Performance.DroppedRequestCount;
	Button->OnPressed.Broadcast();
	TestEqual(TEXT("Destroyed bindings stay silent"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount, Dropped);

	UComboBoxString* ComboBox = NewObject<UComboBoxString>(World);
	UOpenMobileHapticsWidgetComponent* ComboComponent =
		NewObject<UOpenMobileHapticsWidgetComponent>(ComboBox);
	ComboComponent->Initialize(ComboBox);
	ComboComponent->Construct();
	Dropped = Haptics->GetDiagnostics().Performance.DroppedRequestCount;
	ComboBox->OnSelectionChanged.Broadcast(
		TEXT("Direct"),
		ESelectInfo::Direct
	);
	TestEqual(TEXT("Direct selection changes stay silent"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount, Dropped);
	ComboBox->OnSelectionChanged.Broadcast(
		TEXT("Pointer"),
		ESelectInfo::OnMouseClick
	);
	ComboBox->OnSelectionChanged.Broadcast(
		TEXT("Navigation"),
		ESelectInfo::OnNavigation
	);
	TestEqual(TEXT("Pointer and navigation selection both submit"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 2);
	ComboComponent->Destruct();

	USlider* Slider = NewObject<USlider>(World);
	UOpenMobileHapticsWidgetComponent* SliderComponent =
		NewObject<UOpenMobileHapticsWidgetComponent>(Slider);
	SliderComponent->Initialize(Slider);
	SliderComponent->Construct();
	Dropped = Haptics->GetDiagnostics().Performance.DroppedRequestCount;
	Slider->SetValue(0.05f);
	Slider->SetValue(0.11f);
	Slider->SetValue(0.19f);
	Slider->SetValue(0.35f);
	TestEqual(TEXT("Slider submits once per crossed update step"),
		Haptics->GetDiagnostics().Performance.DroppedRequestCount,
		Dropped + 2);
	SliderComponent->Destruct();

	UButton* OrphanButton = NewObject<UButton>();
	UOpenMobileHapticsWidgetComponent* OrphanComponent =
		NewObject<UOpenMobileHapticsWidgetComponent>(OrphanButton);
	OrphanComponent->Initialize(OrphanButton);
	TestFalse(TEXT("Missing subsystem is a safe no-op"),
		OrphanComponent->PlayEffect(OrphanComponent->ButtonPressed));
	OrphanComponent->Destruct();

	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorComponent.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsComponentTestsPrivate
{
	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	FOpenMobileSensorCapability MakeGyroscopeCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::Gyroscope;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("Gyroscope");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Mock;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDeclarativeComponentTest,
	"OpenMobile.Sensors.Blueprint.Component.DeclarativeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDeclarativeComponentTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsComponentTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DeclarativeComponent"));
	Backend.SetSensorCapabilities({MakeGyroscopeCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileSensorsComponentTest"));
	UWorld* World = GameInstance->GetWorld();
	AActor* Owner = World->SpawnActor<AActor>();
	UOpenMobileSensorComponent* Component =
		NewObject<UOpenMobileSensorComponent>(Owner);
	Component->SetAutoActivate(false);
	Component->Sensor = EOpenMobileSensorType::Gyroscope;
	Component->RatePreset = EOpenMobileSensorRatePreset::Game;
	Component->RegisterComponent();
	Component->BeginPlay();
	Component->StartSensor();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();

	UOpenMobileSensorListener* Listener = Component->GetActiveListener();
	TestNotNull(TEXT("The component creates a typed listener"), Listener);
	TestTrue(TEXT("The component listener becomes active"),
		Listener && Listener->IsActive());
	TestEqual(TEXT("The component starts one physical stream"),
		Backend.GetStartSensorStreamCount(), 1);
	if (Listener)
	{
		TestEqual(TEXT("The component uses listener event delivery"),
			Listener->GetAppliedOptions().DeliveryMode,
			EOpenMobileSensorDeliveryMode::EventBatches);
	}

	FOpenMobileVectorSensorSample Sample;
	Sample.Header.Sensor = MakeGyroscopeCapability().Sensor;
	Sample.Header.TimestampSeconds = 1.0;
	Sample.Header.bValid = true;
	Sample.Value = FVector(1.0, 2.0, 3.0);
	FOpenMobileSensorsSampleService::PublishVector(Sample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.0);
	FOpenMobileVectorSensorSample Latest;
	TestTrue(TEXT("The component caches its latest family sample"),
		Component->GetLatestVectorSample(Latest));
	TestEqual(TEXT("The component preserves the typed sample value"),
		Latest.Value, Sample.Value);

	Component->EndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("Owner end play stops the component listener"),
		Listener && Listener->IsActive());
	TestEqual(TEXT("Owner end play stops the physical stream"),
		Backend.GetStopSensorStreamCount(), 1);

	Component->UnregisterComponent();
	Owner->Destroy();
	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

#endif

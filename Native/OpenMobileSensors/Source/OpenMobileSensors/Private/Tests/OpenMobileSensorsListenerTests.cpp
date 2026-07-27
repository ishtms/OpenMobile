#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "UObject/UnrealType.h"

namespace OpenMobileSensorsListenerTestsPrivate
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
	FOpenMobileSensorsGyroscopeListenerTest,
	"OpenMobile.Sensors.Blueprint.Listener.GyroscopeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeListenerTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsListenerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("GyroscopeListener"));
	Backend.SetSensorCapabilities({MakeGyroscopeCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileSensorsListenerTest"));
	UWorld* World = GameInstance->GetWorld();
	USceneComponent* Owner = NewObject<USceneComponent>(World);

	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const FOpenMobileSensorStreamOptions SavedDefaults =
		Settings->DefaultStreamOptions;
	Settings->DefaultStreamOptions.Filters.bEnableLowPass = true;
	Settings->DefaultStreamOptions.Filters.LowPassTimeConstantSeconds = 0.25;

	const FOpenMobileSensorStreamOptions EmptyAdvancedOptions;
	UOpenMobileGyroscopeListener* Listener =
		UOpenMobileGyroscopeListener::ListenForGyroscope(
			World,
			EmptyAdvancedOptions,
			EOpenMobileSensorRatePreset::Game,
			EOpenMobileSensorCoordinateSpace::DeviceFixed,
			false,
			Owner
		);
	TestNotNull(TEXT("The gyroscope listener is created"), Listener);
	if (!Listener)
	{
		Settings->DefaultStreamOptions = SavedDefaults;
		return false;
	}
	Listener->Activate();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("The gyroscope listener becomes active"), Listener->IsActive());
	TestEqual(
		TEXT("The listener event path selects event delivery"),
		Listener->GetAppliedOptions().DeliveryMode,
		EOpenMobileSensorDeliveryMode::EventBatches
	);
	TestTrue(
		TEXT("The listener starts from project filter defaults"),
		Listener->GetAppliedOptions().Filters.bEnableLowPass
	);
	TestEqual(
		TEXT("The listener keeps the project filter constant"),
		Listener->GetAppliedOptions().Filters.LowPassTimeConstantSeconds,
		0.25
	);

	FOpenMobileVectorSensorSample Sample;
	Sample.Header.Sensor = MakeGyroscopeCapability().Sensor;
	Sample.Header.TimestampSeconds = 1.0;
	Sample.Header.bValid = true;
	Sample.Value = FVector(1.0, 2.0, 3.0);
	FOpenMobileSensorsSampleService::PublishVector(Sample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.01);
	FVector AngularVelocity;
	FOpenMobileSensorSampleInfo SampleInfo;
	TestTrue(
		TEXT("The listener caches its latest angular velocity"),
		Listener->GetLatestAngularVelocity(
			AngularVelocity,
			SampleInfo
		)
	);
	TestEqual(
		TEXT("The listener exposes angular velocity in radians per second"),
		AngularVelocity,
		FVector(1.0, 2.0, 3.0)
	);
	TestEqual(TEXT("The compact sample info keeps sequence"), SampleInfo.Sequence, 1ll);

	Owner->MarkAsGarbage();
	TestFalse(
		TEXT("The owner check completes after an owner is destroyed"),
		Listener->TickOwnerForTests()
	);
	TestFalse(TEXT("Owner destruction deactivates the listener"), Listener->IsActive());
	TestEqual(
		TEXT("Owner destruction stops the physical stream"),
		Backend.GetStopSensorStreamCount(),
		1
	);

	Settings->DefaultStreamOptions = SavedDefaults;
	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGyroscopeListenerReflectionTest,
	"OpenMobile.Sensors.Blueprint.Listener.GyroscopeReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeListenerReflectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
#if WITH_METADATA
	const UFunction* Factory =
		UOpenMobileGyroscopeListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileGyroscopeListener,
				ListenForGyroscope
			)
		);
	TestNotNull(TEXT("The gyroscope listener factory is reflected"), Factory);
	if (Factory)
	{
		TestTrue(
			TEXT("The gyroscope listener is searchable by common terms"),
			Factory->GetMetaData(TEXT("Keywords")).Contains(TEXT("gyro"))
		);
		TestTrue(
			TEXT("The full stream options pin is advanced"),
			Factory->GetMetaData(TEXT("AdvancedDisplay")).Contains(
				TEXT("AdvancedOptions")
			)
		);
	}
	const FMulticastDelegateProperty* SampleEvent =
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileGyroscopeListener::StaticClass(),
			GET_MEMBER_NAME_CHECKED(UOpenMobileGyroscopeListener, Sample)
		);
	TestNotNull(TEXT("The listener sample event is reflected"), SampleEvent);
	if (SampleEvent)
	{
		const FProperty* Value = FindFProperty<FProperty>(
			SampleEvent->SignatureFunction,
			TEXT("AngularVelocityRadiansPerSecond")
		);
		TestNotNull(TEXT("The event exposes angular velocity"), Value);
		if (Value)
		{
			TestEqual(
				TEXT("The angular velocity pin states its public unit"),
				Value->GetMetaData(TEXT("DisplayName")),
				FString(TEXT("Angular Velocity (rad/s)"))
			);
		}
	}
#endif
	return true;
}

#endif

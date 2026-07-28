#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorDiscoveryLibrary.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsBlueprintDiscoveryTestsPrivate
{
	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		EOpenMobileCapabilityState State,
		FName RequiredPermission = NAME_None)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = State;
		Capability.RequiredPermission = RequiredPermission;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Mock;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintDiscoveryTest,
	"OpenMobile.Sensors.Blueprint.Discovery.DirectHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintDiscoveryTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBlueprintDiscoveryTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("BlueprintDiscovery"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Gyroscope,
			EOpenMobileCapabilityState::Available),
		MakeCapability(EOpenMobileSensorType::MotionActivity,
			EOpenMobileCapabilityState::Restricted,
			TEXT("MotionActivity")),
		MakeCapability(EOpenMobileSensorType::AmbientLight,
			EOpenMobileCapabilityState::Available)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileSensorsDiscoveryTest"));
	UWorld* World = GameInstance->GetWorld();

	UOpenMobileSensorsSubsystem* Subsystem =
		UOpenMobileSensorDiscoveryLibrary::GetOpenMobileSensorsSubsystem(World);
	TestNotNull(TEXT("Blueprint can get the Sensors subsystem directly"),
		Subsystem);
	FOpenMobileSensorCapability Gyroscope;
	TestTrue(TEXT("Blueprint can query one sensor directly"),
		UOpenMobileSensorDiscoveryLibrary::GetSensorAvailability(
			World, EOpenMobileSensorType::Gyroscope, Gyroscope));
	TestEqual(TEXT("The direct query keeps the selected sensor"),
		Gyroscope.Sensor.Type,
		EOpenMobileSensorType::Gyroscope);
	TestTrue(TEXT("The direct availability helper reports ready hardware"),
		UOpenMobileSensorDiscoveryLibrary::IsSensorAvailable(
			World, EOpenMobileSensorType::Gyroscope));
	const TArray<FOpenMobileSensorIdentifier> Available =
		UOpenMobileSensorDiscoveryLibrary::GetAvailableSensors(World);
	TestTrue(TEXT("Available Sensors includes the gyroscope"),
		Available.Contains(Gyroscope.Sensor));
	FOpenMobileSensorIdentifier Preferred;
	TestTrue(TEXT("Blueprint can select the preferred sensor instance"),
		UOpenMobileSensorDiscoveryLibrary::GetPreferredSensor(
			World, EOpenMobileSensorType::Gyroscope, Preferred));
	TestEqual(TEXT("The preferred instance is the backend default"),
		Preferred.InstanceId,
		FName(TEXT("Default")));

	const FOpenMobileSensorDisplayInfo Display =
		UOpenMobileSensorDiscoveryLibrary::GetSensorDisplayInfo(
			EOpenMobileSensorType::Gyroscope);
	TestEqual(TEXT("Display info names the gyroscope"),
		Display.DisplayName.ToString(),
		FString(TEXT("Gyroscope")));
	TestEqual(TEXT("Display info states the public unit"),
		Display.Unit.ToString(),
		FString(TEXT("rad/s")));
	TestEqual(TEXT("Display info includes the sample family"),
		Display.SampleFamily,
		EOpenMobileSensorSampleFamily::Vector);

	const FOpenMobileSensorAccessRequirement GyroscopeAccess =
		UOpenMobileSensorDiscoveryLibrary::GetRequiredAccessForSensor(
			World, EOpenMobileSensorType::Gyroscope);
	TestEqual(TEXT("Gyroscope requires no user permission"),
		GyroscopeAccess.Requirement,
		EOpenMobileSensorAccessRequirement::None);
	const FOpenMobileSensorAccessRequirement ActivityAccess =
		UOpenMobileSensorDiscoveryLibrary::GetRequiredAccessForSensor(
			World, EOpenMobileSensorType::MotionActivity);
	TestEqual(TEXT("Motion activity reports its sensor permission"),
		ActivityAccess.Requirement,
		EOpenMobileSensorAccessRequirement::SensorPermission);
	TestEqual(TEXT("Motion activity maps to the typed permission"),
		ActivityAccess.Permission,
		EOpenMobileSensorPermission::MotionActivity);

	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const FOpenMobileSensorRatePresetSettings SavedUI = Settings->UIPreset;
	Settings->UIPreset.RequestedFrequencyHz = 17.0;
	const FOpenMobileSensorRatePresetSettings ConfiguredUI =
		UOpenMobileSensorDiscoveryLibrary::GetConfiguredSensorRatePreset(
			EOpenMobileSensorRatePreset::UI);
	TestEqual(TEXT("Rate preview reads the configured UI preset"),
		ConfiguredUI.RequestedFrequencyHz,
		17.0);
	FOpenMobileSensorStreamOptions Requested;
	Requested.RatePreset = EOpenMobileSensorRatePreset::UI;
	FOpenMobileSensorStreamOptions Applied;
	FOpenMobileSensorRateResolution Resolution;
	TestTrue(TEXT("Blueprint can preview resolved options without hardware"),
		UOpenMobileSensorDiscoveryLibrary::PreviewSensorStreamOptions(
			EOpenMobileSensorType::AmbientLight,
			Requested,
			Applied,
			Resolution));
	TestEqual(TEXT("Ambient-light UI preview applies the low-power rate"),
		Applied.CustomFrequencyHz,
		1.0);
	Settings->UIPreset = SavedUI;

	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

#endif

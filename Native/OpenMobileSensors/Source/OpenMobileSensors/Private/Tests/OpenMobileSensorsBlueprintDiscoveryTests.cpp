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
		FName RequiredPermission = NAME_None,
		FName InstanceId = TEXT("Default"))
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = InstanceId;
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
		MakeCapability(EOpenMobileSensorType::Gyroscope,
			EOpenMobileCapabilityState::Available,
			NAME_None,
			TEXT("Secondary")),
		MakeCapability(EOpenMobileSensorType::MotionActivity,
			EOpenMobileCapabilityState::PermissionRequired,
			TEXT("MotionActivity")),
		MakeCapability(EOpenMobileSensorType::BarometricPressure,
			EOpenMobileCapabilityState::TemporarilyUnavailable),
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
	FOpenMobileSensorCapability SecondaryGyroscope;
	TestTrue(TEXT("Blueprint can query a named sensor instance"),
		UOpenMobileSensorDiscoveryLibrary::GetSensorAvailability(
			World,
			EOpenMobileSensorType::Gyroscope,
			SecondaryGyroscope,
			TEXT("Secondary")));
	TestEqual(TEXT("Named lookup returns the requested instance"),
		SecondaryGyroscope.Sensor.InstanceId,
		FName(TEXT("Secondary")));
	const TArray<FOpenMobileSensorIdentifier> Available =
		UOpenMobileSensorDiscoveryLibrary::GetAvailableSensors(World);
	TestTrue(TEXT("Available Sensors includes the gyroscope"),
		Available.Contains(Gyroscope.Sensor));
	FOpenMobileSensorAvailabilityInfo AvailabilityInfo;
	EOpenMobileSensorAvailabilityBranch AvailabilityBranch =
		EOpenMobileSensorAvailabilityBranch::Unavailable;
	UOpenMobileSensorDiscoveryLibrary::BranchSensorAvailability(
		World,
		EOpenMobileSensorType::Gyroscope,
		AvailabilityInfo,
		AvailabilityBranch);
	TestEqual(TEXT("Available sensors use the available branch"),
		AvailabilityBranch,
		EOpenMobileSensorAvailabilityBranch::Available);
	TestTrue(TEXT("Compact availability reports ready state"),
		AvailabilityInfo.bAvailable);
	TestEqual(TEXT("Compact availability reports the minimum rate"),
		AvailabilityInfo.MinimumFrequencyHz,
		1.0);
	UOpenMobileSensorDiscoveryLibrary::BranchSensorAvailability(
		World,
		EOpenMobileSensorType::MotionActivity,
		AvailabilityInfo,
		AvailabilityBranch);
	TestEqual(TEXT("Permissions use the permission-required branch"),
		AvailabilityBranch,
		EOpenMobileSensorAvailabilityBranch::PermissionRequired);
	TestFalse(TEXT("Permission-required availability includes an action"),
		AvailabilityInfo.RequiredAction.IsEmpty());
	UOpenMobileSensorDiscoveryLibrary::BranchSensorAvailability(
		World,
		EOpenMobileSensorType::BarometricPressure,
		AvailabilityInfo,
		AvailabilityBranch);
	TestEqual(TEXT("Transient outages have their own branch"),
		AvailabilityBranch,
		EOpenMobileSensorAvailabilityBranch::TemporarilyUnavailable);
	UOpenMobileSensorDiscoveryLibrary::BranchSensorAvailability(
		World,
		EOpenMobileSensorType::AbsoluteAltitude,
		AvailabilityInfo,
		AvailabilityBranch);
	TestEqual(TEXT("Missing sensors use the unavailable branch"),
		AvailabilityBranch,
		EOpenMobileSensorAvailabilityBranch::Unavailable);
#if WITH_METADATA
	const UFunction* SubsystemGetter =
		UOpenMobileSensorDiscoveryLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorDiscoveryLibrary,
				GetOpenMobileSensorsSubsystem));
	TestNotNull(TEXT("The branded subsystem getter is reflected"),
		SubsystemGetter);
	if (SubsystemGetter)
	{
		TestEqual(TEXT("The getter hides its world context"),
			SubsystemGetter->GetMetaData(TEXT("WorldContext")),
			FString(TEXT("WorldContextObject")));
		const FString Keywords =
			SubsystemGetter->GetMetaData(TEXT("Keywords"));
		TestTrue(TEXT("The getter is searchable as mobile sensors"),
			Keywords.Contains(TEXT("mobile sensors")));
		TestTrue(TEXT("The getter is searchable by accelerometer"),
			Keywords.Contains(TEXT("accelerometer")));
		TestTrue(TEXT("The getter is searchable by gyroscope"),
			Keywords.Contains(TEXT("gyroscope")));
	}
	const UFunction* AvailabilityFunction =
		UOpenMobileSensorDiscoveryLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorDiscoveryLibrary,
				BranchSensorAvailability));
	TestNotNull(TEXT("The availability branch is reflected"),
		AvailabilityFunction);
	if (AvailabilityFunction)
	{
		TestEqual(TEXT("Availability states expand into execution pins"),
			AvailabilityFunction->GetMetaData(TEXT("ExpandEnumAsExecs")),
			FString(TEXT("Branch")));
	}
#endif
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

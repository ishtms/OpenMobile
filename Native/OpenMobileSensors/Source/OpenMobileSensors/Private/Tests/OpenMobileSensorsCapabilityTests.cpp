#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsCapabilityTestsPrivate
{
	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		EOpenMobileSensorAvailabilitySource Source =
			EOpenMobileSensorAvailabilitySource::Native,
		EOpenMobileCapabilityState State =
			EOpenMobileCapabilityState::Available
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = State;
		Capability.Source = Source;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 200.0;
		Capability.bSupportsNativeBatching = true;
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Supported;
		return Capability;
	}

	const FOpenMobileSensorCapability* FindCapability(
		const FOpenMobileSensorCapabilitySnapshot& Snapshot,
		EOpenMobileSensorType Type
	)
	{
		return Snapshot.Sensors.FindByPredicate(
			[Type](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type == Type;
			}
		);
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsFullySupportedMatrixTest,
	"OpenMobile.Sensors.Capabilities.FullySupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsFullySupportedMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Full"));
	TArray<FOpenMobileSensorCapability> Capabilities;
	for (EOpenMobileSensorType Type : FOpenMobileSensorTypes::GetAll())
	{
		Capabilities.Add(MakeCapability(Type));
	}
	Backend.SetSensorCapabilities(MoveTemp(Capabilities));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(true);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Matrix includes every stable sensor type"),
		Snapshot.Sensors.Num(), FOpenMobileSensorTypes::GetAll().Num());
	TSet<FName> StableNames;
	for (const FOpenMobileSensorCapability& Capability : Snapshot.Sensors)
	{
		TestEqual(TEXT("Fully supported sensor is available"),
			Capability.Availability.State,
			EOpenMobileCapabilityState::Available);
		TestEqual(TEXT("Fully supported sensor is native"),
			Capability.Source,
			EOpenMobileSensorAvailabilitySource::Native);
		TestTrue(TEXT("Supported rates are ordered"),
			Capability.MinimumFrequencyHz > 0.0
				&& Capability.MaximumFrequencyHz >=
					Capability.MinimumFrequencyHz);
		TestTrue(TEXT("Native batching is reported"),
			Capability.bSupportsNativeBatching);
		StableNames.Add(Capability.Availability.Name);
	}
	TestEqual(TEXT("Stable capability names are unique"),
		StableNames.Num(), Snapshot.Sensors.Num());
	TestEqual(TEXT("Backend matrix is queried once"),
		Backend.GetSensorCapabilityQueryCount(), 1);
	FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Repeated snapshots reuse the cached backend matrix"),
		Backend.GetSensorCapabilityQueryCount(), 1);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPartiallySupportedMatrixTest,
	"OpenMobile.Sensors.Capabilities.PartiallySupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPartiallySupportedMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Partial"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Accelerometer = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Accelerometer
	);
	const FOpenMobileSensorCapability* Gyroscope = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Gyroscope
	);
	TestNotNull(TEXT("Supported sensor remains in the matrix"), Accelerometer);
	TestNotNull(TEXT("Missing sensor remains in the matrix"), Gyroscope);
	TestEqual(TEXT("Supported sensor remains available"),
		Accelerometer->Availability.State,
		EOpenMobileCapabilityState::Available);
	TestEqual(TEXT("Missing hardware is unavailable"),
		Gyroscope->Availability.State,
		EOpenMobileCapabilityState::Unavailable);
	TestEqual(TEXT("Missing hardware has an explicit restriction"),
		Gyroscope->ActiveRestriction,
		EOpenMobileSensorRestriction::MissingHardware);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPermissionBlockedMatrixTest,
	"OpenMobile.Sensors.Capabilities.PermissionBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPermissionBlockedMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Permission"));
	FOpenMobileSensorCapability Activity = MakeCapability(
		EOpenMobileSensorType::MotionActivity
	);
	Activity.RequiredPermission =
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity
		);
	Backend.SetSensorCapabilities({Activity});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Denied
	);
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Blocked = FindCapability(
		Snapshot,
		EOpenMobileSensorType::MotionActivity
	);
	TestEqual(TEXT("Denied permission blocks the capability"),
		Blocked->Availability.State,
		EOpenMobileCapabilityState::Denied);
	TestEqual(TEXT("Permission restriction remains explicit"),
		Blocked->ActiveRestriction,
		EOpenMobileSensorRestriction::Permission);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Granted
	);
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Granted permission restores availability"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::MotionActivity
		)->Availability.State,
		EOpenMobileCapabilityState::Available);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDerivedOnlyMatrixTest,
	"OpenMobile.Sensors.Capabilities.DerivedOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedOnlyMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Derived"));
	FOpenMobileSensorCapability Gravity = MakeCapability(
		EOpenMobileSensorType::Gravity,
		EOpenMobileSensorAvailabilitySource::Derived
	);
	FOpenMobileSensorCapability TrueHeading = MakeCapability(
		EOpenMobileSensorType::TrueHeading,
		EOpenMobileSensorAvailabilitySource::Derived
	);
	Backend.SetSensorCapabilities({Gravity, TrueHeading});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Derived source remains explicit"),
		FindCapability(Snapshot, EOpenMobileSensorType::Gravity)->Source,
		EOpenMobileSensorAvailabilitySource::Derived);
	TestEqual(TEXT("True heading starts with missing input"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::TrueHeading
		)->ActiveRestriction,
		EOpenMobileSensorRestriction::MissingInput);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	FOpenMobileSensorLocationInput LocationInput;
	LocationInput.TimestampSeconds = 1.0;
	Subsystem->SetTrueHeadingLocationInputNative(LocationInput);
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Location input restores derived true heading"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::TrueHeading
		)->Availability.State,
		EOpenMobileCapabilityState::Available);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRateLimitedMatrixTest,
	"OpenMobile.Sensors.Capabilities.RateLimited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRateLimitedMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RateLimited"));
	FOpenMobileSensorCapability Accelerometer = MakeCapability(
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorAvailabilitySource::Native,
		EOpenMobileCapabilityState::TemporarilyUnavailable
	);
	Accelerometer.ActiveRestriction =
		EOpenMobileSensorRestriction::RateLimited;
	Accelerometer.MaximumFrequencyHz = 50.0;
	Backend.SetSensorCapabilities({Accelerometer});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Capability = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Accelerometer
	);
	TestEqual(TEXT("Rate limit remains a distinct restriction"),
		Capability->ActiveRestriction,
		EOpenMobileSensorRestriction::RateLimited);
	TestEqual(TEXT("Rate-limited maximum is preserved"),
		Capability->MaximumFrequencyHz, 50.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMockAndReplayMatrixTest,
	"OpenMobile.Sensors.Capabilities.MockAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMockAndReplayMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DevelopmentSources"));
	Backend.SetSensorCapabilities({
		MakeCapability(
			EOpenMobileSensorType::Accelerometer,
			EOpenMobileSensorAvailabilitySource::Mock
		),
		MakeCapability(
			EOpenMobileSensorType::Gyroscope,
			EOpenMobileSensorAvailabilitySource::Replay
		)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Mock source is visible"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::Accelerometer
		)->Source,
		EOpenMobileSensorAvailabilitySource::Mock);
	TestEqual(TEXT("Replay source is visible"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::Gyroscope
		)->Source,
		EOpenMobileSensorAvailabilitySource::Replay);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsUnsupportedEditorMatrixTest,
	"OpenMobile.Sensors.Capabilities.UnsupportedEditor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsUnsupportedEditorMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Unsupported host still returns the complete matrix"),
		Snapshot.Sensors.Num(), FOpenMobileSensorTypes::GetAll().Num());
	TestEqual(TEXT("Unsupported host reports no backend"),
		Snapshot.BackendAvailability.State,
		EOpenMobileCapabilityState::NotSupported);
	for (const FOpenMobileSensorCapability& Capability : Snapshot.Sensors)
	{
		TestEqual(TEXT("Unsupported host marks every sensor unsupported"),
			Capability.Availability.State,
			EOpenMobileCapabilityState::NotSupported);
	}
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMaterialChangeEventsTest,
	"OpenMobile.Sensors.Capabilities.MaterialChangeEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMaterialChangeEventsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsCapabilityService::GetSnapshot();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	int32 ServiceEvents = 0;
	int32 SubsystemEvents = 0;
	const FDelegateHandle ServiceHandle =
		FOpenMobileSensorsCapabilityService::OnChanged().AddLambda(
			[&ServiceEvents](const FOpenMobileSensorCapabilitySnapshot&)
			{
				++ServiceEvents;
			}
		);
	const FDelegateHandle SubsystemHandle =
		Subsystem->OnCapabilitiesChangedNative().AddLambda(
			[&SubsystemEvents](const FOpenMobileSensorCapabilitySnapshot&)
			{
				++SubsystemEvents;
			}
		);

	FOpenMobileSensorsMockBackend Backend(TEXT("Events"));
	FOpenMobileSensorCapability Accelerometer = MakeCapability(
		EOpenMobileSensorType::Accelerometer
	);
	Accelerometer.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::Suspended;
	FOpenMobileSensorCapability Activity = MakeCapability(
		EOpenMobileSensorType::MotionActivity
	);
	Activity.RequiredPermission =
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity
		);
	FOpenMobileSensorCapability TrueHeading = MakeCapability(
		EOpenMobileSensorType::TrueHeading,
		EOpenMobileSensorAvailabilitySource::Derived
	);
	Backend.SetSensorCapabilities({Accelerometer, Activity, TrueHeading});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	TestEqual(TEXT("Backend registration emits one material event"),
		ServiceEvents, 1);
	FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Repeated snapshot emits no event"), ServiceEvents, 1);

	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Denied
	);
	TestEqual(TEXT("Permission change emits one event"), ServiceEvents, 2);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Denied
	);
	TestEqual(TEXT("Duplicate permission state is coalesced"), ServiceEvents, 2);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Granted
	);
	TestEqual(TEXT("Permission recovery emits one event"), ServiceEvents, 3);

	FOpenMobileSensorsCapabilityService::SetApplicationActive(false);
	TestEqual(TEXT("Lifecycle restriction emits one event"), ServiceEvents, 4);
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Inactive lifecycle suspends unsupported background delivery"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::Accelerometer
		)->ActiveRestriction,
		EOpenMobileSensorRestriction::Background);
	FOpenMobileSensorsCapabilityService::SetApplicationActive(false);
	TestEqual(TEXT("Duplicate lifecycle state is coalesced"), ServiceEvents, 4);
	FOpenMobileSensorsCapabilityService::SetApplicationActive(true);
	TestEqual(TEXT("Foreground recovery emits one event"), ServiceEvents, 5);
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Foreground restores sensor availability"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::Accelerometer
		)->Availability.State,
		EOpenMobileCapabilityState::Available);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(true);
	TestEqual(TEXT("Location input emits one event"), ServiceEvents, 6);
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Location input restores true heading"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::TrueHeading
		)->Availability.State,
		EOpenMobileCapabilityState::Available);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(true);
	TestEqual(TEXT("Duplicate location input is coalesced"), ServiceEvents, 6);
	TestEqual(TEXT("Subsystem receives every material service event"),
		SubsystemEvents, ServiceEvents);

	FOpenMobileSensorsCapabilityService::OnChanged().Remove(ServiceHandle);
	Subsystem->OnCapabilitiesChangedNative().Remove(SubsystemHandle);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

#endif

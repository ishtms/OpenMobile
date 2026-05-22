#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsModule.h"
#include "OpenMobileSensorsPermissionPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBackendRegistryTest,
	"OpenMobile.Sensors.Architecture.BackendRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBackendRegistryTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsBackendRegistry::ResetForTests();

	const FOpenMobileCapability Unsupported =
		FOpenMobileSensorsModule::GetBackendCapability();
	TestEqual(
		TEXT("No backend has a stable state"),
		Unsupported.State,
		EOpenMobileCapabilityState::NotSupported
	);
	TestEqual(
		TEXT("No backend has a stable detail"),
		Unsupported.Detail,
		FString(TEXT("No OpenMobile Sensors backend is registered."))
	);

	FOpenMobileSensorsMockBackend Lower(TEXT("Lower"), 10);
	FOpenMobileSensorsMockBackend Higher(TEXT("Higher"), 20);
	TestTrue(
		TEXT("Lower-priority backend registers"),
		FOpenMobileSensorsBackendRegistry::RegisterBackend(Lower)
	);
	const FOpenMobileSensorsBackendToken LowerToken =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	TestTrue(TEXT("Registered backend produces a token"), LowerToken.Generation > 0);

	TestTrue(
		TEXT("Higher-priority backend registers"),
		FOpenMobileSensorsBackendRegistry::RegisterBackend(Higher)
	);
	TestTrue(
		TEXT("Backend lookup is lazy and priority based"),
		FOpenMobileSensorsBackendRegistry::FindBackend() == &Higher
	);
	TestFalse(
		TEXT("A registry change invalidates older work"),
		FOpenMobileSensorsBackendRegistry::IsTokenCurrent(LowerToken)
	);

	FOpenMobileSensorsMockBackend Duplicate(TEXT("Higher"), 30);
	TestFalse(
		TEXT("Duplicate backend names are rejected"),
		FOpenMobileSensorsBackendRegistry::RegisterBackend(Duplicate)
	);
	TestTrue(
		TEXT("Higher-priority backend unregisters"),
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Higher)
	);
	TestTrue(TEXT("Backend shutdown is called once"), Higher.WasShutdown());
	TestTrue(
		TEXT("Lookup falls back without cached module order"),
		FOpenMobileSensorsBackendRegistry::FindBackend() == &Lower
	);
	FOpenMobileSensorsMockBackend Alphabetical(TEXT("Alphabetical"), 10);
	TestTrue(
		TEXT("Equal-priority backend registers"),
		FOpenMobileSensorsBackendRegistry::RegisterBackend(Alphabetical)
	);
	TestTrue(
		TEXT("Equal priorities use a stable name tie break"),
		FOpenMobileSensorsBackendRegistry::FindBackend() == &Alphabetical
	);
	TestTrue(
		TEXT("Equal-priority backend unregisters"),
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Alphabetical)
	);
	TestTrue(
		TEXT("Lower-priority backend unregisters"),
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Lower)
	);

	FOpenMobileSensorsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMockScriptTest,
	"OpenMobile.Sensors.Architecture.MockScript",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMockScriptTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsMockBackend Mock;

	FOpenMobileCapability Capability;
	Capability.Name = TEXT("OpenMobile.Sensors.Accelerometer");
	Capability.State = EOpenMobileCapabilityState::Available;
	Mock.AddCapability(Capability);
	Mock.AddSampleBatch({FVector3d(1.0, 2.0, 3.0)});
	Mock.AddPermission(
		TEXT("MotionActivity"),
		EOpenMobileCapabilityState::PermissionRequired
	);
	Mock.AddLifecycle(false);
	Mock.AddDelay(0.5);
	Mock.AddError(FOpenMobileError::Make(
		EOpenMobileErrorCode::Unavailable,
		TEXT("Scripted failure")
	));

	TArray<EOpenMobileSensorsMockEventType> Delivered;
	auto Capture = [&Delivered](const FOpenMobileSensorsMockEvent& Event)
	{
		Delivered.Add(Event.Type);
	};
	Mock.AdvanceScript(0.0, Capture);
	TestEqual(TEXT("Events before the delay are delivered"), Delivered.Num(), 4);
	Mock.AdvanceScript(0.49, Capture);
	TestEqual(TEXT("Delayed callback remains pending"), Delivered.Num(), 4);
	Mock.AdvanceScript(0.01, Capture);
	TestEqual(TEXT("Delayed callback is delivered once"), Delivered.Num(), 5);
	TestEqual(
		TEXT("The delayed event preserves its type"),
		Delivered.Last(),
		EOpenMobileSensorsMockEventType::Error
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPermissionOwnershipTest,
	"OpenMobile.Sensors.Architecture.PermissionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPermissionOwnershipTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const TArray<FName> Permissions = {
		FOpenMobileSensorsPermissionPolicy::MotionActivity(),
		FOpenMobileSensorsPermissionPolicy::ActivityRecognition(),
		FOpenMobileSensorsPermissionPolicy::TrueHeadingLocation()
	};
	TSet<FName> UniquePermissions;
	for (FName Permission : Permissions)
	{
		UniquePermissions.Add(Permission);
	}
	TestEqual(
		TEXT("Sensor permission and prerequisite names are unique"),
		UniquePermissions.Num(),
		Permissions.Num()
	);
	for (FName Permission : Permissions)
	{
		TestTrue(
			TEXT("Sensor permission is recognized"),
			FOpenMobileSensorsPermissionPolicy::IsSensorPermission(Permission)
		);
		TestFalse(
			TEXT("Sensor permission has an explanation"),
			FOpenMobileSensorsPermissionPolicy::GetExplanation(Permission).IsEmpty()
		);
	}
	TestFalse(
		TEXT("Unknown permission is not sensor owned"),
		FOpenMobileSensorsPermissionPolicy::IsSensorPermission(
			TEXT("OpenMobile.Other.Permission")
		)
	);
	return true;
}

#endif

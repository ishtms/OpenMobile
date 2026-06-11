#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsModule.h"
#include "OpenMobileSensorsPermissionPolicy.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSubscriptionOwnershipTest,
	"OpenMobile.Sensors.Architecture.SubscriptionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSubscriptionOwnershipTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsMockBackend Backend(TEXT("Ownership"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* FirstGameInstance = NewObject<UGameInstance>();
	UGameInstance* SecondGameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* FirstSubsystem =
		NewObject<UOpenMobileSensorsSubsystem>(FirstGameInstance);
	UOpenMobileSensorsSubsystem* SecondSubsystem =
		NewObject<UOpenMobileSensorsSubsystem>(SecondGameInstance);
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Request.Sensor.InstanceId = TEXT("Default");

	const FOpenMobileSensorSubscriptionResult First =
		FirstSubsystem->StartSubscriptionNative(Request);
	const FOpenMobileSensorSubscriptionResult Second =
		SecondSubsystem->StartSubscriptionNative(Request);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(
		TEXT("First owner receives an accepted handle"),
		First.Operation.Code,
		EOpenMobileSensorResultCode::Accepted
	);
	TestEqual(
		TEXT("Second owner receives an accepted handle"),
		Second.Operation.Code,
		EOpenMobileSensorResultCode::Accepted
	);
	TestTrue(TEXT("First handle is valid"), First.Handle.IsValid());
	TestTrue(TEXT("Second handle is valid"), Second.Handle.IsValid());
	TestTrue(
		TEXT("Logical subscriptions have unique identifiers"),
		First.Handle.GetIdentifier() != Second.Handle.GetIdentifier()
	);
	TestEqual(
		TEXT("Same-sensor owners share one process service"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(Request.Sensor),
		2
	);
	TestEqual(
		TEXT("First update succeeds"),
		FirstSubsystem->UpdateSubscriptionNative(
			First.Handle,
			Request.Options
		).Code,
		EOpenMobileSensorResultCode::Success
	);
	TestEqual(
		TEXT("Repeated update is idempotent"),
		FirstSubsystem->UpdateSubscriptionNative(
			First.Handle,
			Request.Options
		).Code,
		EOpenMobileSensorResultCode::Success
	);

	const FOpenMobileSensorOperationResult CrossOwnerStop =
		FirstSubsystem->StopSubscriptionNative(Second.Handle);
	TestEqual(
		TEXT("A different owner cannot stop the handle"),
		CrossOwnerStop.Code,
		EOpenMobileSensorResultCode::InvalidHandle
	);
	FOpenMobileSensorSubscriptionStateSnapshot SecondState;
	TestTrue(
		TEXT("Cross-owner stop leaves the subscription active"),
		SecondSubsystem->GetSubscriptionStateNative(Second.Handle, SecondState)
	);
	TestEqual(
		TEXT("Cross-owner stop changes no process subscription"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(Request.Sensor),
		2
	);
	int32 FlushCompletionCount = 0;
	FOpenMobileSensorFlushResult FlushResult;
	FirstSubsystem->FlushNative(
		Second.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&FlushCompletionCount, &FlushResult](
				const FOpenMobileSensorFlushResult& Result
			)
			{
				++FlushCompletionCount;
				FlushResult = Result;
			}
		)
	);
	TestEqual(TEXT("Cross-owner flush completes once"), FlushCompletionCount, 1);
	TestEqual(
		TEXT("Cross-owner flush returns invalid handle"),
		FlushResult.Operation.Code,
		EOpenMobileSensorResultCode::InvalidHandle
	);
	SecondSubsystem->FlushNative(
		Second.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&FlushCompletionCount, &FlushResult](
				const FOpenMobileSensorFlushResult& Result
			)
			{
				++FlushCompletionCount;
				FlushResult = Result;
			}
		)
	);
	TestEqual(TEXT("Owned flush completes once"), FlushCompletionCount, 2);
	TestEqual(
		TEXT("Owned handle reaches the backend flush contract"),
		FlushResult.Operation.Code,
		EOpenMobileSensorResultCode::NotSupported
	);

	const FOpenMobileSensorOperationResult FirstStop =
		FirstSubsystem->StopSubscriptionNative(First.Handle);
	TestEqual(
		TEXT("The owning system can stop its handle"),
		FirstStop.Code,
		EOpenMobileSensorResultCode::Success
	);
	const FOpenMobileSensorOperationResult DuplicateStop =
		FirstSubsystem->StopSubscriptionNative(First.Handle);
	TestEqual(
		TEXT("Duplicate stop returns a typed invalid-handle result"),
		DuplicateStop.Code,
		EOpenMobileSensorResultCode::InvalidHandle
	);
	TestTrue(
		TEXT("Stopping one logical subscriber preserves the other"),
		SecondSubsystem->GetSubscriptionStateNative(Second.Handle, SecondState)
	);
	TestEqual(
		TEXT("One same-sensor subscriber remains"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(Request.Sensor),
		1
	);

	const FOpenMobileSensorSubscriptionResult Replacement =
		FirstSubsystem->StartSubscriptionNative(Request);
	TestTrue(TEXT("Replacement handle is valid"), Replacement.Handle.IsValid());
	TestTrue(
		TEXT("Replacement uses a later generation"),
		Replacement.Handle.GetGeneration() != First.Handle.GetGeneration()
	);
	FOpenMobileSensorSubscriptionStateSnapshot StaleState;
	TestFalse(
		TEXT("Stopped handle stays stale after replacement"),
		FirstSubsystem->GetSubscriptionStateNative(First.Handle, StaleState)
	);
	FOpenMobileSensorSubscriptionRequest PermissionRequest = Request;
	PermissionRequest.Sensor.Type = EOpenMobileSensorType::MotionActivity;
	const FOpenMobileSensorSubscriptionResult PermissionHandle =
		FirstSubsystem->StartSubscriptionNative(PermissionRequest);
	FOpenMobileSensorsSubscriptionService::
		InvalidateForUnrecoverablePermissionLoss(
			EOpenMobileSensorType::MotionActivity
		);
	TestFalse(
		TEXT("Unrecoverable permission loss invalidates matching handles"),
		FirstSubsystem->GetSubscriptionStateNative(
			PermissionHandle.Handle,
			StaleState
		)
	);
	TestTrue(
		TEXT("Permission loss preserves unrelated sensor handles"),
		FirstSubsystem->GetSubscriptionStateNative(
			Replacement.Handle,
			StaleState
		)
	);

	TestEqual(
		TEXT("Stop-all removes only the first owner's remaining handle"),
		FirstSubsystem->StopAllSubscriptionsNative(),
		1
	);
	TestEqual(
		TEXT("Repeated stop-all is idempotent"),
		FirstSubsystem->StopAllSubscriptionsNative(),
		0
	);
	TestTrue(
		TEXT("Second owner survives first-owner stop-all"),
		SecondSubsystem->GetSubscriptionStateNative(Second.Handle, SecondState)
	);
	const FOpenMobileSensorSubscriptionResult TeardownHandle =
		FirstSubsystem->StartSubscriptionNative(Request);
	TestTrue(
		TEXT("PIE owner accepts a replacement before teardown"),
		TeardownHandle.Handle.IsValid()
	);
	FirstSubsystem->Deinitialize();
	FirstSubsystem->Deinitialize();
	TestFalse(
		TEXT("Repeated PIE teardown invalidates its remaining handle once"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			TeardownHandle.Handle
		)
	);
	TestEqual(
		TEXT("PIE teardown preserves the other owner's subscription"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(Request.Sensor),
		1
	);
	TestTrue(
		TEXT("Second owner stays queryable after PIE teardown"),
		SecondSubsystem->GetSubscriptionStateNative(Second.Handle, SecondState)
	);

	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	TestFalse(
		TEXT("Backend loss invalidates accepted handles"),
		SecondSubsystem->GetSubscriptionStateNative(Second.Handle, SecondState)
	);
	const FOpenMobileSensorOperationResult AfterBackendLoss =
		SecondSubsystem->UpdateSubscriptionNative(
			Second.Handle,
			Request.Options
		);
	TestEqual(
		TEXT("Backend loss returns a typed stale-handle result"),
		AfterBackendLoss.Code,
		EOpenMobileSensorResultCode::InvalidHandle
	);
	TestFalse(
		TEXT("Late batches cannot use an invalid backend generation"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(Second.Handle)
	);
	FOpenMobileSensorsMockBackend ReplacementBackend(TEXT("Replacement"), 100);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(ReplacementBackend);
	const FOpenMobileSensorSubscriptionResult AfterReplacement =
		SecondSubsystem->StartSubscriptionNative(Request);
	TestTrue(
		TEXT("Replacement backend accepts a new generation"),
		AfterReplacement.Handle.IsValid()
	);
	FOpenMobileSensorsMockBackend HigherBackend(TEXT("Higher"), 200);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(HigherBackend);
	TestFalse(
		TEXT("Backend replacement invalidates the earlier generation"),
		SecondSubsystem->GetSubscriptionStateNative(
			AfterReplacement.Handle,
			SecondState
		)
	);
	const FOpenMobileSensorSubscriptionResult BeforeShutdown =
		SecondSubsystem->StartSubscriptionNative(Request);
	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	TestFalse(
		TEXT("Module shutdown invalidates every handle"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			BeforeShutdown.Handle
		)
	);
	TestEqual(
		TEXT("Shutdown rejects new subscriptions"),
		SecondSubsystem->StartSubscriptionNative(Request).Operation.Code,
		EOpenMobileSensorResultCode::Unavailable
	);

	SecondSubsystem->Deinitialize();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(HigherBackend);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(ReplacementBackend);
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	return true;
}

#endif

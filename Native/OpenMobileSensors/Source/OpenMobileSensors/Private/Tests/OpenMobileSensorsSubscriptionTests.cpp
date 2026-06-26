#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsSubscriptionTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		double FrequencyHz = 60.0,
		double CallbackFrequencyHz = 30.0,
		EOpenMobileSensorType Type = EOpenMobileSensorType::Accelerometer
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = Type;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = FrequencyHz;
		Request.Options.MaximumCallbackFrequencyHz = CallbackFrequencyHz;
		return Request;
	}

	FOpenMobileSensorCapability MakeAttitudeCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeRequest(
			60.0,
			30.0,
			EOpenMobileSensorType::Attitude
		).Sensor;
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		return Capability;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSubscriptionRequestValidationTest,
	"OpenMobile.Sensors.Subscriptions.RequestValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSubscriptionRequestValidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSubscriptionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Validation"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	Request.Sensor.Type = EOpenMobileSensorType::Unknown;
	TestEqual(TEXT("Unknown sensors are rejected"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		).Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	Request = MakeRequest();
	Request.Options.CustomFrequencyHz =
		std::numeric_limits<double>::quiet_NaN();
	TestEqual(TEXT("Non-finite sampling rates are rejected"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		).Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	Request = MakeRequest();
	Request.Options.BufferCapacitySamples = 0;
	TestEqual(TEXT("Unbounded buffer policies are rejected"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		).Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	Request = MakeRequest();
	Request.Options.Filters.bEnableLowPass = true;
	Request.Options.Filters.LowPassTimeConstantSeconds = -1.0;
	TestEqual(TEXT("Invalid filter policies are rejected"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		).Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	Request = MakeRequest();
	Request.Options.DeliveryMode =
		static_cast<EOpenMobileSensorDeliveryMode>(255);
	TestEqual(TEXT("Invalid delivery modes are rejected"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		).Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	const FOpenMobileSensorSubscriptionResult Accepted =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	TestEqual(TEXT("A valid request is accepted synchronously"),
		Accepted.Operation.Code,
		EOpenMobileSensorResultCode::Accepted);
	TestEqual(TEXT("Validation never starts native hardware"),
		Backend.GetStartSensorStreamCount(), 0);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Accepted.Handle
	);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSubscriptionStartStateEventsTest,
	"OpenMobile.Sensors.Subscriptions.StartStateEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSubscriptionStartStateEventsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSubscriptionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("States"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	TArray<EOpenMobileSensorSubscriptionState> States;
	Subsystem->OnSubscriptionStateChangedNative().AddLambda(
		[&States](const FOpenMobileSensorSubscriptionStateSnapshot& Snapshot)
		{
			States.Add(Snapshot.State);
		}
	);
	const FOpenMobileSensorSubscriptionResult Accepted =
		Subsystem->StartSubscriptionNative(MakeRequest());
	FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
	TestEqual(TEXT("Common acceptance returns before native start"),
		Backend.GetStartSensorStreamCount(), 0);
	TestTrue(TEXT("Accepted state is queryable"),
		Subsystem->GetSubscriptionStateNative(Accepted.Handle, Snapshot));
	TestEqual(TEXT("The initial state is accepted"),
		Snapshot.State,
		EOpenMobileSensorSubscriptionState::Accepted);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Native start is issued once"),
		Backend.GetStartSensorStreamCount(), 1);
	TestEqual(TEXT("Starting and active events are forwarded"),
		States.Num(), 2);
	if (States.Num() == 2)
	{
		TestEqual(TEXT("Starting is reported first"),
			States[0], EOpenMobileSensorSubscriptionState::Starting);
		TestEqual(TEXT("Active is reported after backend success"),
			States[1], EOpenMobileSensorSubscriptionState::Active);
	}
	TestTrue(TEXT("Active state remains queryable"),
		Subsystem->GetSubscriptionStateNative(Accepted.Handle, Snapshot));
	TestEqual(TEXT("Successful backend start activates the handle"),
		Snapshot.State, EOpenMobileSensorSubscriptionState::Active);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSubscriptionStartFailureTest,
	"OpenMobile.Sensors.Subscriptions.StartFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSubscriptionStartFailureTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSubscriptionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Failure"));
	Backend.SetStartSensorStreamResult(FOpenMobileSensorsErrorMapper::Map(
		EOpenMobileSensorFailureReason::OperationalFailure,
		TEXT("Mock"),
		TEXT("StartFailed")
	));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	TArray<EOpenMobileSensorSubscriptionState> States;
	const FDelegateHandle EventHandle =
		FOpenMobileSensorsSubscriptionService::OnStateChanged().AddLambda(
			[&States, Owner](
				const FGuid& ChangedOwner,
				const FOpenMobileSensorSubscriptionStateSnapshot& Snapshot
			)
			{
				if (ChangedOwner == Owner)
				{
					States.Add(Snapshot.State);
				}
			}
		);
	const FOpenMobileSensorSubscriptionResult Accepted =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	TestTrue(TEXT("The common service still returns an accepted handle"),
		Accepted.Handle.IsValid());
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
	TestTrue(TEXT("Failed starts remain queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Accepted.Handle,
			Snapshot
		));
	TestEqual(TEXT("Native failure moves the handle to failed"),
		Snapshot.State,
		EOpenMobileSensorSubscriptionState::Failed);
	TestTrue(TEXT("Failed state carries the normalized error"),
		Snapshot.Error.IsSet());
	TestEqual(TEXT("No physical stream survives start failure"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		0);
	TestTrue(TEXT("Failure is reported through the state event"),
		States.Contains(EOpenMobileSensorSubscriptionState::Failed));
	FOpenMobileSensorsSubscriptionService::OnStateChanged().Remove(EventHandle);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Accepted.Handle
	);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSubscriptionRateNegotiationTest,
	"OpenMobile.Sensors.Subscriptions.RateNegotiation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSubscriptionRateNegotiationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSubscriptionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Negotiation"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid SlowOwner = FGuid::NewGuid();
	const FGuid FastOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Slow =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			SlowOwner,
			MakeRequest(15.0, 15.0)
		);
	const FOpenMobileSensorSubscriptionResult Fast =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FastOwner,
			MakeRequest(120.0, 60.0)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Compatible subscribers share one physical start"),
		Backend.GetStartSensorStreamCount(), 1);
	TestEqual(TEXT("One shared physical stream remains"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		1);
	TestEqual(TEXT("The shared stream uses the fastest requested rate"),
		Backend.GetLastStartedPhysicalRequest().RequestedFrequencyHz,
		120.0);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		FastOwner,
		Fast.Handle
	);
	TestEqual(TEXT("Removing the fastest subscriber renegotiates once"),
		Backend.GetReconfigureSensorStreamCount(), 1);
	TestEqual(TEXT("The remaining rate becomes the physical rate"),
		Backend.GetLastReconfiguredPhysicalRequest().RequestedFrequencyHz,
		15.0);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		SlowOwner,
		Slow.Handle
	);
	TestEqual(TEXT("The last subscriber tears down the native stream"),
		Backend.GetStopSensorStreamCount(), 1);
	TestEqual(TEXT("No physical streams remain after last teardown"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSubscriptionIncompatibleOptionsTest,
	"OpenMobile.Sensors.Subscriptions.IncompatibleOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSubscriptionIncompatibleOptionsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSubscriptionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Compatibility"));
	Backend.SetSensorCapabilities({MakeAttitudeCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorSubscriptionRequest GameRelative = MakeRequest(
		60.0,
		30.0,
		EOpenMobileSensorType::Attitude
	);
	FOpenMobileSensorSubscriptionRequest MagneticNorth = GameRelative;
	MagneticNorth.Options.AttitudeReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::MagneticNorth;
	FOpenMobileSensorsSubscriptionService::StartSubscription(
		FGuid::NewGuid(),
		GameRelative
	);
	FOpenMobileSensorsSubscriptionService::StartSubscription(
		FGuid::NewGuid(),
		MagneticNorth
	);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Incompatible reference frames do not coalesce"),
		Backend.GetStartSensorStreamCount(), 2);
	TestEqual(TEXT("Each incompatible request owns a physical stream"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		2);
	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	TestEqual(TEXT("Shutdown tears down both physical streams"),
		Backend.GetStopSensorStreamCount(), 2);
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSubscriptionUpdateInPlaceTest,
	"OpenMobile.Sensors.Subscriptions.UpdateInPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSubscriptionUpdateInPlaceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSubscriptionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Update"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Accepted =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(30.0, 15.0)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorStreamOptions Updated = MakeRequest(90.0, 45.0).Options;
	Updated.BufferCapacitySamples = 512;
	Updated.CoordinateSpace = EOpenMobileSensorCoordinateSpace::CurrentScreen;
	Updated.Filters.bEnableLowPass = true;
	Updated.Filters.LowPassTimeConstantSeconds = 0.25;
	const FOpenMobileSensorOperationResult Update =
		FOpenMobileSensorsSubscriptionService::UpdateSubscription(
			Owner,
			Accepted.Handle,
			Updated
		);
	TestEqual(TEXT("A safe native reconfiguration succeeds"),
		Update.Code, EOpenMobileSensorResultCode::Success);
	TestEqual(TEXT("The physical stream is reconfigured once"),
		Backend.GetReconfigureSensorStreamCount(), 1);
	FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
	TestTrue(TEXT("The original handle remains current"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Accepted.Handle,
			Snapshot
		));
	TestEqual(TEXT("Updates preserve handle identity"),
		Snapshot.Handle, Accepted.Handle);
	TestEqual(TEXT("The updated callback cap is retained"),
		Snapshot.AppliedOptions.MaximumCallbackFrequencyHz, 45.0);
	TestEqual(TEXT("The updated buffer capacity is retained"),
		Snapshot.AppliedOptions.BufferCapacitySamples, 512);
	TestTrue(TEXT("The updated filter policy is retained"),
		Snapshot.AppliedOptions.Filters.bEnableLowPass);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Accepted.Handle
	);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSubscriptionIndependentFanoutTest,
	"OpenMobile.Sensors.Subscriptions.IndependentFanout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSubscriptionIndependentFanoutTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSubscriptionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Fanout"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorIdentifier Sensor = MakeRequest().Sensor;
	const FOpenMobileSensorSubscriptionResult Ui =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest(100.0, 10.0)
		);
	const FOpenMobileSensorSubscriptionResult Game =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest(100.0, 100.0)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TArray<FOpenMobileSensorSubscriptionHandle> Due =
		FOpenMobileSensorsSubscriptionService::SelectSubscribersForSample(
			Sensor,
			0.0
		);
	TestEqual(TEXT("The first sample reaches both subscribers"), Due.Num(), 2);
	Due = FOpenMobileSensorsSubscriptionService::SelectSubscribersForSample(
		Sensor,
		0.02
	);
	TestEqual(TEXT("Only the faster callback is due after 20 ms"),
		Due.Num(), 1);
	TestTrue(TEXT("The game subscriber receives the faster callback"),
		Due.Contains(Game.Handle));
	TestFalse(TEXT("The UI subscriber is independently downsampled"),
		Due.Contains(Ui.Handle));
	Due = FOpenMobileSensorsSubscriptionService::SelectSubscribersForSample(
		Sensor,
		0.1
	);
	TestEqual(TEXT("Both subscribers are due at the UI interval"),
		Due.Num(), 2);
	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPhysicalStreamFailureTest,
	"OpenMobile.Sensors.Subscriptions.PhysicalStreamFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPhysicalStreamFailureTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSubscriptionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PhysicalFailure"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid FirstOwner = FGuid::NewGuid();
	const FGuid SecondOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult First =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FirstOwner,
			MakeRequest()
		);
	const FOpenMobileSensorSubscriptionResult Second =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			SecondOwner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorOperationResult Failure =
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable,
			TEXT("Android"),
			TEXT("SensorDisconnected")
		);
	TestTrue(TEXT("The active physical generation accepts its failure"),
		FOpenMobileSensorsSubscriptionService::FailPhysicalStreamFromBackend(
			Token,
			Backend.GetLastStartedPhysicalHandle(),
			Failure
		));
	FOpenMobileSensorSubscriptionStateSnapshot FirstState;
	FOpenMobileSensorSubscriptionStateSnapshot SecondState;
	TestTrue(TEXT("The first failed handle remains queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			FirstOwner,
			First.Handle,
			FirstState
		));
	TestTrue(TEXT("The second failed handle remains queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			SecondOwner,
			Second.Handle,
			SecondState
		));
	TestEqual(TEXT("All shared subscribers fail"),
		FirstState.State,
		EOpenMobileSensorSubscriptionState::Failed);
	TestEqual(TEXT("The shared failure is preserved"),
		SecondState.Error.NativeCode,
		FString(TEXT("SensorDisconnected")));
	TestEqual(TEXT("The lost physical stream is removed"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		0);
	TestFalse(TEXT("A repeated physical failure is rejected"),
		FOpenMobileSensorsSubscriptionService::FailPhysicalStreamFromBackend(
			Token,
			Backend.GetLastStartedPhysicalHandle(),
			Failure
		));
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		FirstOwner,
		First.Handle
	);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		SecondOwner,
		Second.Handle
	);
	FinishBackend(Backend);
	return true;
}

#endif

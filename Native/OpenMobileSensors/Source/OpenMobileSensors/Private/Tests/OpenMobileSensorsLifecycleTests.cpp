#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsLifecycleTestsPrivate
{
	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		EOpenMobileSensorBackgroundSupport BackgroundSupport
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 120.0;
		Capability.BackgroundSupport = BackgroundSupport;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeAccelerometerRequest(
		double FrequencyHz = 60.0
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = FrequencyHz;
		Request.Options.MaximumCallbackFrequencyHz = FrequencyHz;
		Request.Options.Filters.bEnableLowPass = true;
		Request.Options.Filters.LowPassTimeConstantSeconds = 1.0;
		return Request;
	}

	FOpenMobileSensorSubscriptionRequest MakeLifecycleRequest(
		EOpenMobileSensorType Type,
		EOpenMobileSensorLifecyclePolicy Policy
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = Type;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 10.0;
		Request.Options.MaximumCallbackFrequencyHz = 10.0;
		Request.Options.LifecyclePolicy = Policy;
		return Request;
	}

	FOpenMobileVectorSensorSample MakeAccelerometerSample(
		double TimestampSeconds,
		double Value
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
		);
		Sample.Value = FVector(Value, 0.0, 0.0);
		return Sample;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
		FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLifecycleSuspendResumeTest,
	"OpenMobile.Sensors.Lifecycle.SuspendResumeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLifecycleSuspendResumeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLifecycleTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("LifecycleSuspendResume"));
	Backend.SetSensorCapabilities({MakeCapability(
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorBackgroundSupport::Suspended
	)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid FirstOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult First =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FirstOwner,
			MakeAccelerometerRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FGuid PeerOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Peer =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			PeerOwner,
			MakeAccelerometerRequest(30.0)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(
		TEXT("Process-level ownership shares one native stream"),
		Backend.GetStartSensorStreamCount(),
		1
	);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeAccelerometerSample(1.0, 0.0)
	);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeAccelerometerSample(2.0, 10.0)
	);

	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
	FOpenMobileSensorSubscriptionStateSnapshot State;
	TestTrue(
		TEXT("The logical handle remains queryable while interrupted"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			FirstOwner,
			First.Handle,
			State
		)
	);
	TestEqual(
		TEXT("A foreground stream pauses on focus loss"),
		State.State,
		EOpenMobileSensorSubscriptionState::Paused
	);
	TestTrue(
		TEXT("The paused handle remains current"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			FirstOwner,
			First.Handle
		)
	);
	TestEqual(
		TEXT("The final paused subscriber releases its native stream"),
		Backend.GetStopSensorStreamCount(),
		1
	);
	TestEqual(
		TEXT("No physical stream remains while suspended"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		0
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Sample;
	TestTrue(
		TEXT("The last sample remains readable while paused"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			FirstOwner,
			First.Handle,
			0,
			3.0,
			Read,
			Sample
		)
	);
	TestEqual(
		TEXT("Paused reads are marked explicitly"),
		Read.Status,
		EOpenMobileSensorReadStatus::Paused
	);

	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	TestEqual(
		TEXT("Repeated inactive events do not stop twice"),
		Backend.GetStopSensorStreamCount(),
		1
	);
	TestTrue(
		TEXT("A peer owner can exit while both handles are paused"),
		FOpenMobileSensorsSubscriptionService::StopSubscription(
			PeerOwner,
			Peer.Handle
		).IsSuccess()
	);
	FOpenMobileSensorStreamOptions ResumedOptions =
		MakeAccelerometerRequest(90.0).Options;
	TestTrue(
		TEXT("A paused subscription accepts updated options"),
		FOpenMobileSensorsSubscriptionService::UpdateSubscription(
			FirstOwner,
			First.Handle,
			ResumedOptions
		).IsSuccess()
	);

	const FGuid SecondOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult AddedWhileInactive =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			SecondOwner,
			MakeAccelerometerRequest(30.0)
		);
	TestTrue(
		TEXT("A foreground-only request is retained while inactive"),
		AddedWhileInactive.Operation.IsSuccess()
	);
	TestTrue(
		TEXT("The inactive request has a logical state"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			SecondOwner,
			AddedWhileInactive.Handle,
			State
		)
	);
	TestEqual(
		TEXT("A request added while inactive starts paused"),
		State.State,
		EOpenMobileSensorSubscriptionState::Paused
	);
	TestTrue(
		TEXT("An owner can stop a paused request"),
		FOpenMobileSensorsSubscriptionService::StopSubscription(
			SecondOwner,
			AddedWhileInactive.Handle
		).IsSuccess()
	);

	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		FirstOwner,
		First.Handle,
		State
	);
	TestEqual(
		TEXT("Foreground entry waits for interruption recovery"),
		State.State,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		FirstOwner,
		First.Handle,
		State
	);
	TestEqual(
		TEXT("The still-owned handle resumes"),
		State.State,
		EOpenMobileSensorSubscriptionState::Active
	);
	TestEqual(
		TEXT("Resume creates a new native stream"),
		Backend.GetStartSensorStreamCount(),
		2
	);
	TestEqual(
		TEXT("Resume renegotiates the current requested rate"),
		Backend.GetLastStartedPhysicalRequest().RequestedFrequencyHz,
		90.0
	);
	TestFalse(
		TEXT("The stopped inactive request is not restored"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			SecondOwner,
			AddedWhileInactive.Handle
		)
	);
	TestFalse(
		TEXT("The stopped peer owner is not restored"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			PeerOwner,
			Peer.Handle
		)
	);

	FOpenMobileSensorsSampleService::PublishVector(
		MakeAccelerometerSample(3.0, 30.0)
	);
	TestTrue(
		TEXT("The resumed subscription receives new samples"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			FirstOwner,
			First.Handle,
			0,
			3.0,
			Read,
			Sample
		)
	);
	TestEqual(
		TEXT("Resume resets timing-sensitive filter state"),
		Sample.Value,
		FVector(30.0, 0.0, 0.0)
	);

	FOpenMobileSensorsSubscriptionService::StopSubscription(
		FirstOwner,
		First.Handle
	);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLifecycleOperationPoliciesTest,
	"OpenMobile.Sensors.Lifecycle.OperationPolicies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLifecycleOperationPoliciesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLifecycleTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bPreviousBackgroundOptIn =
		Settings->bAllowBackgroundSensorDelivery;
	Settings->bAllowBackgroundSensorDelivery = true;
	FOpenMobileSensorsMockBackend Backend(TEXT("LifecyclePolicies"));
	Backend.SetSensorCapabilities({
		MakeCapability(
			EOpenMobileSensorType::Accelerometer,
			EOpenMobileSensorBackgroundSupport::Suspended
		),
		MakeCapability(
			EOpenMobileSensorType::Magnetometer,
			EOpenMobileSensorBackgroundSupport::Suspended
		),
		MakeCapability(
			EOpenMobileSensorType::StepCounter,
			EOpenMobileSensorBackgroundSupport::EventDriven
		),
		MakeCapability(
			EOpenMobileSensorType::ActivityTransition,
			EOpenMobileSensorBackgroundSupport::EventDriven
		),
		MakeCapability(
			EOpenMobileSensorType::Proximity,
			EOpenMobileSensorBackgroundSupport::Suspended
		),
		MakeCapability(
			EOpenMobileSensorType::AbsoluteAltitude,
			EOpenMobileSensorBackgroundSupport::Supported
		)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	auto Start = [&Owner](
		EOpenMobileSensorType Type,
		EOpenMobileSensorLifecyclePolicy Policy
	)
	{
		return FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeLifecycleRequest(Type, Policy)
		);
	};
	const FOpenMobileSensorSubscriptionResult Raw = Start(
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorLifecyclePolicy::SuspendInBackground
	);
	const FOpenMobileSensorSubscriptionResult Stop = Start(
		EOpenMobileSensorType::Magnetometer,
		EOpenMobileSensorLifecyclePolicy::StopInBackground
	);
	const FOpenMobileSensorSubscriptionResult Steps = Start(
		EOpenMobileSensorType::StepCounter,
		EOpenMobileSensorLifecyclePolicy::ContinueWhenSupported
	);
	const FOpenMobileSensorSubscriptionResult Transitions = Start(
		EOpenMobileSensorType::ActivityTransition,
		EOpenMobileSensorLifecyclePolicy::ContinueWhenSupported
	);
	const FOpenMobileSensorSubscriptionResult Proximity = Start(
		EOpenMobileSensorType::Proximity,
		EOpenMobileSensorLifecyclePolicy::ContinueWhenSupported
	);
	const FOpenMobileSensorSubscriptionResult Altitude = Start(
		EOpenMobileSensorType::AbsoluteAltitude,
		EOpenMobileSensorLifecyclePolicy::ContinueWhenSupported
	);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(
		TEXT("Each operation starts its own physical stream"),
		Backend.GetStartSensorStreamCount(),
		6
	);

	bool bSawLifecycleStop = false;
	EOpenMobileSensorFailureReason StopReason =
		EOpenMobileSensorFailureReason::None;
	const FDelegateHandle StateHandle =
		FOpenMobileSensorsSubscriptionService::OnStateChanged().AddLambda(
			[&](
				const FGuid& ChangedOwner,
				const FOpenMobileSensorSubscriptionStateSnapshot& Snapshot
			)
			{
				if (ChangedOwner == Owner
					&& Snapshot.Handle == Stop.Handle
					&& Snapshot.State ==
						EOpenMobileSensorSubscriptionState::Stopped)
				{
					bSawLifecycleStop = true;
					StopReason = Snapshot.Failure.Reason;
				}
			}
		);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();

	auto TestState = [this, &Owner](
		const TCHAR* What,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorSubscriptionState Expected
	)
	{
		FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
		TestTrue(
			FString::Printf(TEXT("%s remains queryable"), What),
			FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
				Owner,
				Handle,
				Snapshot
			)
		);
		TestEqual(What, Snapshot.State, Expected);
	};
	TestState(
		TEXT("Raw motion suspends"),
		Raw.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	TestState(
		TEXT("Event-driven steps continue"),
		Steps.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	TestState(
		TEXT("Activity transitions continue"),
		Transitions.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	TestState(
		TEXT("Unsupported proximity continuation suspends"),
		Proximity.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	TestState(
		TEXT("Supported altitude continuation remains active"),
		Altitude.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	TestTrue(
		TEXT("Stop-in-background emits a terminal state"),
		bSawLifecycleStop
	);
	TestEqual(
		TEXT("Lifecycle stop reports its reason"),
		StopReason,
		EOpenMobileSensorFailureReason::BackgroundRestricted
	);
	TestFalse(
		TEXT("Stop-in-background invalidates its handle"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			Owner,
			Stop.Handle
		)
	);
	TestEqual(
		TEXT("Only continuing physical streams remain"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		3
	);
	TestEqual(
		TEXT("Suspended and stopped operations release native streams"),
		Backend.GetStopSensorStreamCount(),
		3
	);
	FOpenMobileSensorStreamOptions AltitudeOptions =
		MakeLifecycleRequest(
			EOpenMobileSensorType::AbsoluteAltitude,
			EOpenMobileSensorLifecyclePolicy::SuspendInBackground
		).Options;
	TestTrue(
		TEXT("An active background operation accepts a safer policy"),
		FOpenMobileSensorsSubscriptionService::UpdateSubscription(
			Owner,
			Altitude.Handle,
			AltitudeOptions
		).IsSuccess()
	);
	TestState(
		TEXT("The updated altitude policy suspends immediately"),
		Altitude.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorStreamOptions ProximityOptions =
		MakeLifecycleRequest(
			EOpenMobileSensorType::Proximity,
			EOpenMobileSensorLifecyclePolicy::StopInBackground
		).Options;
	TestTrue(
		TEXT("A paused operation accepts stop-in-background"),
		FOpenMobileSensorsSubscriptionService::UpdateSubscription(
			Owner,
			Proximity.Handle,
			ProximityOptions
		).IsSuccess()
	);
	TestFalse(
		TEXT("The updated stop policy invalidates the paused handle"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			Owner,
			Proximity.Handle
		)
	);
	TestEqual(
		TEXT("Only event-driven continuations remain after policy updates"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		2
	);
	TestEqual(
		TEXT("The newly suspended altitude releases its native stream"),
		Backend.GetStopSensorStreamCount(),
		4
	);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	TestEqual(
		TEXT("Repeated background events are idempotent"),
		Backend.GetStopSensorStreamCount(),
		4
	);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(
		TEXT("Only suspended operations restart"),
		Backend.GetStartSensorStreamCount(),
		8
	);
	TestFalse(
		TEXT("The stopped operation is never restored"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			Owner,
			Stop.Handle
		)
	);

	FOpenMobileSensorsSubscriptionService::OnStateChanged().Remove(
		StateHandle
	);
	FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(Owner);
	Settings->bAllowBackgroundSensorDelivery = bPreviousBackgroundOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingLifecycleDefaultTest,
	"OpenMobile.Sensors.Lifecycle.RecordingPolicyDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingLifecycleDefaultTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorRecordingOptions Options;
	TestEqual(
		TEXT("Recordings suspend and resume by default"),
		Options.LifecyclePolicy,
		EOpenMobileSensorLifecyclePolicy::SuspendInBackground
	);
	Options.LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::StopInBackground;
	TestEqual(
		TEXT("Recordings accept an explicit lifecycle policy"),
		Options.LifecyclePolicy,
		EOpenMobileSensorLifecyclePolicy::StopInBackground
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLifecycleStartInterruptionRaceTest,
	"OpenMobile.Sensors.Lifecycle.StartInterruptionRace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLifecycleStartInterruptionRaceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLifecycleTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("LifecycleStartRace"));
	Backend.SetSensorCapabilities({MakeCapability(
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorBackgroundSupport::Suspended
	)});
	bool bInterruptedFirstStart = false;
	Backend.SetStartSensorStreamHookForTests([&]()
	{
		if (!bInterruptedFirstStart)
		{
			bInterruptedFirstStart = true;
			FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
		}
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeAccelerometerRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorSubscriptionStateSnapshot State;
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		Owner,
		Subscription.Handle,
		State
	);
	TestEqual(
		TEXT("An interruption during native start leaves the handle paused"),
		State.State,
		EOpenMobileSensorSubscriptionState::Paused
	);
	TestEqual(
		TEXT("The raced native stream is released"),
		Backend.GetStopSensorStreamCount(),
		1
	);
	TestEqual(
		TEXT("No raced physical stream remains registered"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		0
	);

	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		Owner,
		Subscription.Handle,
		State
	);
	TestEqual(
		TEXT("The interrupted start recovers with the same handle"),
		State.State,
		EOpenMobileSensorSubscriptionState::Active
	);
	TestEqual(
		TEXT("Recovery starts a replacement native stream"),
		Backend.GetStartSensorStreamCount(),
		2
	);

	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	TestEqual(
		TEXT("Shutdown releases the recovered native stream"),
		Backend.GetStopSensorStreamCount(),
		2
	);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(
		TEXT("Late lifecycle events cannot restart after shutdown"),
		Backend.GetStartSensorStreamCount(),
		2
	);
	TestFalse(
		TEXT("Shutdown invalidates the logical handle"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			Owner,
			Subscription.Handle
		)
	);

	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLifecycleCapabilitySignalsTest,
	"OpenMobile.Sensors.Lifecycle.CapabilitySignals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLifecycleCapabilitySignalsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLifecycleTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("LifecycleCapabilities"));
	Backend.SetSensorCapabilities({MakeCapability(
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorBackgroundSupport::Suspended
	)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);

	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Accelerometer = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Accelerometer
	);
	TestEqual(
		TEXT("Focus loss restricts foreground-only capability"),
		Accelerometer ? Accelerometer->ActiveRestriction :
			EOpenMobileSensorRestriction::None,
		EOpenMobileSensorRestriction::Background
	);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	Accelerometer = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Accelerometer
	);
	TestEqual(
		TEXT("Foreground entry waits for focus recovery in capabilities"),
		Accelerometer ? Accelerometer->ActiveRestriction :
			EOpenMobileSensorRestriction::None,
		EOpenMobileSensorRestriction::Background
	);

	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	Accelerometer = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Accelerometer
	);
	TestEqual(
		TEXT("Full lifecycle recovery restores capability"),
		Accelerometer ? Accelerometer->Availability.State :
			EOpenMobileCapabilityState::NotSupported,
		EOpenMobileCapabilityState::Available
	);
	FinishBackend(Backend);
	return true;
}

#endif

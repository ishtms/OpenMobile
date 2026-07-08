#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileProximityMonitoringPolicy.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsProximityTestsPrivate
{
	FOpenMobileSensorIdentifier MakeSensor()
	{
		FOpenMobileSensorIdentifier Sensor;
		Sensor.Type = EOpenMobileSensorType::Proximity;
		Sensor.InstanceId = TEXT("Default");
		return Sensor;
	}

	FOpenMobileSensorCapability MakeCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeSensor();
		Capability.Availability.Name = TEXT("Proximity");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Suspended;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorDeliveryMode DeliveryMode
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor = MakeSensor();
		Request.Options.DeliveryMode = DeliveryMode;
		return Request;
	}

	FOpenMobileProximitySensorSample MakeSample(
		double TimestampSeconds,
		bool bNear
	)
	{
		FOpenMobileProximitySensorSample Sample;
		Sample.Header.Sensor = MakeSensor();
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Raw
		);
		Sample.bNear = bNear;
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
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsProximityMonitoringPolicyTest,
	"OpenMobile.Sensors.Proximity.MonitoringPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsProximityMonitoringPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileProximityMonitoringPolicy Policy;
	const FOpenMobileProximityMonitoringAction First =
		Policy.Acquire(false, true);
	TestTrue(TEXT("First lease enables proximity monitoring"),
		First.bShouldSetMonitoringEnabled);
	TestTrue(TEXT("First lease requests enabled monitoring"),
		First.bMonitoringEnabled);
	Policy.ObserveMonitoringEnabled(true);
	const FOpenMobileProximityMonitoringAction Second =
		Policy.Acquire(true, true);
	TestFalse(TEXT("Another lease does not toggle monitoring"),
		Second.bShouldSetMonitoringEnabled);
	TestEqual(TEXT("Both leases are retained"), Policy.GetLeaseCount(), 2);
	TestFalse(TEXT("One release keeps shared monitoring enabled"),
		Policy.Release().bShouldSetMonitoringEnabled);

	const FOpenMobileProximityMonitoringAction Paused =
		Policy.SetApplicationActive(false);
	TestTrue(TEXT("Pause restores monitoring disabled"),
		Paused.bShouldSetMonitoringEnabled);
	TestFalse(TEXT("Pause restores the original disabled state"),
		Paused.bMonitoringEnabled);
	Policy.ObserveMonitoringEnabled(false);
	const FOpenMobileProximityMonitoringAction Resumed =
		Policy.SetApplicationActive(true);
	TestTrue(TEXT("Resume reacquires proximity monitoring"),
		Resumed.bShouldSetMonitoringEnabled);
	TestTrue(TEXT("Resume enables proximity monitoring"),
		Resumed.bMonitoringEnabled);
	Policy.ObserveMonitoringEnabled(true);
	const FOpenMobileProximityMonitoringAction LastRelease = Policy.Release();
	TestTrue(TEXT("Last release restores monitoring"),
		LastRelease.bShouldSetMonitoringEnabled);
	TestFalse(TEXT("Last release restores the original disabled state"),
		LastRelease.bMonitoringEnabled);
	TestEqual(TEXT("All monitoring leases are released"),
		Policy.GetLeaseCount(), 0);

	FOpenMobileProximityMonitoringPolicy ExistingOwner;
	TestFalse(TEXT("Existing monitoring is not re-enabled"),
		ExistingOwner.Acquire(true, true).bShouldSetMonitoringEnabled);
	TestFalse(TEXT("Pause does not disable another owner"),
		ExistingOwner.SetApplicationActive(false)
			.bShouldSetMonitoringEnabled);
	TestFalse(TEXT("Shutdown preserves another owner's monitoring"),
		ExistingOwner.Shutdown().bShouldSetMonitoringEnabled);

	FOpenMobileProximityMonitoringPolicy ShutdownOwner;
	ShutdownOwner.Acquire(false, true);
	ShutdownOwner.ObserveMonitoringEnabled(true);
	const FOpenMobileProximityMonitoringAction Shutdown =
		ShutdownOwner.Shutdown();
	TestTrue(TEXT("Shutdown restores monitoring when OpenMobile enabled it"),
		Shutdown.bShouldSetMonitoringEnabled);
	TestFalse(TEXT("Shutdown restores the original disabled state"),
		Shutdown.bMonitoringEnabled);
	TestEqual(TEXT("Shutdown releases every monitoring lease"),
		ShutdownOwner.GetLeaseCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsProximityTransitionsAndLifecycleTest,
	"OpenMobile.Sensors.Proximity.TransitionsAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsProximityTransitionsAndLifecycleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsProximityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ProximityLifecycle"));
	Backend.SetSensorCapabilities({MakeCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid EventOwner = FGuid::NewGuid();
	const FGuid PollOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Event =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			EventOwner,
			MakeRequest(EOpenMobileSensorDeliveryMode::EventBatches)
		);
	const FOpenMobileSensorSubscriptionResult Poll =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			PollOwner,
			MakeRequest(EOpenMobileSensorDeliveryMode::LatestValue)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("Event proximity subscription starts"),
		Event.Operation.IsSuccess());
	TestTrue(TEXT("Polling proximity subscription starts"),
		Poll.Operation.IsSuccess());
	TestEqual(TEXT("Proximity subscribers share one physical stream"),
		Backend.GetStartSensorStreamCount(), 1);
	TArray<bool> EventStates;
	FOpenMobileSensorsSampleService::OnProximityBatch().AddLambda(
		[&](
			const FGuid& Owner,
			const FOpenMobileSensorSubscriptionHandle& Handle,
			const FOpenMobileProximitySensorBatch& Batch
		)
		{
			if (Owner != EventOwner || Handle != Event.Handle)
			{
				return;
			}
			for (const FOpenMobileProximitySensorSample& Sample : Batch.Samples)
			{
				EventStates.Add(Sample.bNear);
			}
		}
	);
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	FOpenMobileProximitySensorBatch NearBatch;
	NearBatch.Samples.Add(MakeSample(1.0, true));
	TestTrue(TEXT("Near state enters the common sample path"),
		FOpenMobileSensorsSampleService::PublishProximityBatchFromBackend(
			Token,
			PhysicalHandle,
			NearBatch
		));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.0);
	FOpenMobileSensorReadResult Read;
	FOpenMobileProximitySensorSample Latest;
	TestTrue(TEXT("Near state is available to polling"),
		FOpenMobileSensorsSampleService::ReadLatestProximity(
			PollOwner,
			Poll.Handle,
			0,
			1.0,
			Read,
			Latest
		));
	TestTrue(TEXT("Polled proximity is near"), Latest.bNear);
	TestFalse(TEXT("State-only proximity has no distance"),
		Latest.bHasDistanceMeters);
	TestFalse(TEXT("State-only proximity has no maximum range"),
		Latest.bHasMaximumRangeMeters);

	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Poll.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileProximitySensorBatch FarWhilePaused;
	FarWhilePaused.Samples.Add(MakeSample(2.0, false));
	FOpenMobileSensorsSampleService::PublishProximityBatchFromBackend(
		Token,
		PhysicalHandle,
		FarWhilePaused
	);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(2.0);
	FOpenMobileSensorsSampleService::ReadLatestProximity(
		PollOwner,
		Poll.Handle,
		0,
		2.0,
		Read,
		Latest
	);
	TestTrue(TEXT("Paused polling keeps the prior state"), Latest.bNear);
	TestEqual(TEXT("Paused proximity read is explicit"),
		Read.Status, EOpenMobileSensorReadStatus::Paused);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Poll.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileProximitySensorBatch FarAfterResume;
	FarAfterResume.Samples.Add(MakeSample(3.0, false));
	FOpenMobileSensorsSampleService::PublishProximityBatchFromBackend(
		Token,
		PhysicalHandle,
		FarAfterResume
	);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(3.0);
	FOpenMobileSensorsSampleService::ReadLatestProximity(
		PollOwner,
		Poll.Handle,
		0,
		3.0,
		Read,
		Latest
	);
	TestFalse(TEXT("Polling accepts far state after resume"), Latest.bNear);
	TestEqual(TEXT("Near and far transitions both emit events"),
		EventStates.Num(), 3);
	if (EventStates.Num() == 3)
	{
		TestTrue(TEXT("First event is near"), EventStates[0]);
		TestFalse(TEXT("Paused peer does not suppress the far event"),
			EventStates[1]);
		TestFalse(TEXT("Resumed far state remains explicit"),
			EventStates[2]);
	}

	FOpenMobileSensorsSubscriptionService::StopSubscription(
		EventOwner,
		Event.Handle
	);
	TestEqual(TEXT("Stopping one subscriber keeps monitoring alive"),
		Backend.GetStopSensorStreamCount(), 0);
	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	TestEqual(TEXT("Shutdown stops the remaining physical stream once"),
		Backend.GetStopSensorStreamCount(), 1);
	TestEqual(TEXT("Shutdown clears proximity subscriptions"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(), 0);
	FinishBackend(Backend);

	ResetServices();
	FOpenMobileSensorsMockBackend Missing(TEXT("ProximityMissing"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Missing);
	const FOpenMobileSensorSubscriptionResult Unavailable =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest(EOpenMobileSensorDeliveryMode::LatestValue)
		);
	TestEqual(TEXT("Missing proximity hardware is rejected"),
		Unavailable.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	TestEqual(TEXT("Missing proximity hardware has a typed reason"),
		Unavailable.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::MissingHardware);
	TestEqual(TEXT("Missing proximity hardware starts no native stream"),
		Missing.GetStartSensorStreamCount(), 0);
	FinishBackend(Missing);
	return true;
}

#endif

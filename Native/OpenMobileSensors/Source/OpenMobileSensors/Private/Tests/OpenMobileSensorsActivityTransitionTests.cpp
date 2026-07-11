#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileActivityTransitionTracker.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsActivityTransitionTestsPrivate
{
	FOpenMobileActivitySensorSample MakeClassification(
		EOpenMobileMotionActivity Activity,
		EOpenMobileActivityConfidence Confidence,
		double TimestampSeconds,
		FName Provider = TEXT("TestProvider")
	)
	{
		FOpenMobileActivitySensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::MotionActivity;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Activity = Activity;
		Sample.Confidence = Confidence;
		Sample.ConcurrentActivities = {Activity};
		Sample.ActivityProvider = Provider;
		return Sample;
	}

	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		EOpenMobileCapabilityState State =
			EOpenMobileCapabilityState::Available,
		EOpenMobileSensorAvailabilitySource Source =
			EOpenMobileSensorAvailabilitySource::Native
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = State;
		Capability.Source = Source;
		return Capability;
	}

	FOpenMobileActivitySensorSample MakeNativeTransition(
		EOpenMobileMotionActivity Activity,
		EOpenMobileActivityTransition Transition,
		double TimestampSeconds
	)
	{
		FOpenMobileActivitySensorSample Sample;
		Sample.Header.Sensor.Type =
			EOpenMobileSensorType::ActivityTransition;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Activity = Activity;
		Sample.Confidence = EOpenMobileActivityConfidence::High;
		Sample.Transition = Transition;
		Sample.TransitionOrigin =
			EOpenMobileActivityTransitionOrigin::Native;
		Sample.ActivityProvider = TEXT("AndroidProvider");
		return Sample;
	}

	FOpenMobileSensorSubscriptionRequest MakeTransitionRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::ActivityTransition;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.DeliveryMode =
			EOpenMobileSensorDeliveryMode::Buffered;
		return Request;
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
	FOpenMobileSensorsDerivedActivityTransitionOrderingTest,
	"OpenMobile.Sensors.Activity.Transitions.DerivedOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedActivityTransitionOrderingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsActivityTransitionTestsPrivate;
	FOpenMobileActivityTransitionTracker Tracker;
	Tracker.Configure(0.25);
	FOpenMobileActivitySensorBatch Batch;
	TestFalse(TEXT("The first classification establishes a baseline"),
		Tracker.Process(MakeClassification(
			EOpenMobileMotionActivity::Walking,
			EOpenMobileActivityConfidence::Medium,
			1.0
		), Batch));
	TestTrue(TEXT("A later activity change emits transitions"),
		Tracker.Process(MakeClassification(
			EOpenMobileMotionActivity::Running,
			EOpenMobileActivityConfidence::High,
			2.0
		), Batch));
	TestEqual(TEXT("A known-to-known change emits a pair"),
		Batch.Samples.Num(), 2);
	if (Batch.Samples.Num() == 2)
	{
		TestEqual(TEXT("The stopped transition is ordered first"),
			Batch.Samples[0].Transition,
			EOpenMobileActivityTransition::Stopped);
		TestEqual(TEXT("The previous activity stops"),
			Batch.Samples[0].Activity,
			EOpenMobileMotionActivity::Walking);
		TestEqual(TEXT("Stopped confidence comes from the previous state"),
			Batch.Samples[0].Confidence,
			EOpenMobileActivityConfidence::Medium);
		TestEqual(TEXT("The started transition is ordered second"),
			Batch.Samples[1].Transition,
			EOpenMobileActivityTransition::Started);
		TestEqual(TEXT("The current activity starts"),
			Batch.Samples[1].Activity,
			EOpenMobileMotionActivity::Running);
		TestEqual(TEXT("The provider is preserved"),
			Batch.Samples[1].ActivityProvider,
			FName(TEXT("TestProvider")));
		TestEqual(TEXT("The transition is marked derived"),
			Batch.Samples[1].TransitionOrigin,
			EOpenMobileActivityTransitionOrigin::Derived);
		TestTrue(TEXT("Derived provenance replaces native provenance"),
			(Batch.Samples[1].Header.SourceFlags & static_cast<int32>(
				EOpenMobileSensorSourceFlags::PluginDerived
			)) != 0);
		TestFalse(TEXT("Derived provenance does not claim native fusion"),
			(Batch.Samples[1].Header.SourceFlags & static_cast<int32>(
				EOpenMobileSensorSourceFlags::NativeFused
			)) != 0);
	}
	TestFalse(TEXT("A duplicate classification emits no transition"),
		Tracker.Process(MakeClassification(
			EOpenMobileMotionActivity::Running,
			EOpenMobileActivityConfidence::High,
			2.5
		), Batch));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDerivedActivityTransitionDebounceTest,
	"OpenMobile.Sensors.Activity.Transitions.DebounceAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedActivityTransitionDebounceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsActivityTransitionTestsPrivate;
	FOpenMobileActivityTransitionTracker Tracker;
	Tracker.Configure(0.25);
	FOpenMobileActivitySensorBatch Batch;
	Tracker.Process(MakeClassification(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::High,
		1.0
	), Batch);
	TestTrue(TEXT("The first change commits immediately"),
		Tracker.Process(MakeClassification(
			EOpenMobileMotionActivity::Running,
			EOpenMobileActivityConfidence::High,
			2.0
		), Batch));
	TestFalse(TEXT("A rapid reversal is debounced"),
		Tracker.Process(MakeClassification(
			EOpenMobileMotionActivity::Walking,
			EOpenMobileActivityConfidence::High,
			2.1
		), Batch));
	TestTrue(TEXT("The same reversal commits after the debounce window"),
		Tracker.Process(MakeClassification(
			EOpenMobileMotionActivity::Walking,
			EOpenMobileActivityConfidence::High,
			2.3
		), Batch));
	FOpenMobileActivitySensorSample Resumed = MakeClassification(
		EOpenMobileMotionActivity::Running,
		EOpenMobileActivityConfidence::High,
		3.0
	);
	Resumed.Header.bStatefulProcessingReset = true;
	TestFalse(TEXT("Resume establishes a fresh baseline"),
		Tracker.Process(Resumed, Batch));
	FOpenMobileActivitySensorSample Delayed = MakeClassification(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::High,
		2.8
	);
	Delayed.Header.SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::Replay
	);
	TestFalse(TEXT("Delayed history after resume is suppressed"),
		Tracker.Process(Delayed, Batch));
	TestTrue(TEXT("A new post-resume change emits once"),
		Tracker.Process(MakeClassification(
			EOpenMobileMotionActivity::Walking,
			EOpenMobileActivityConfidence::High,
			3.5
		), Batch));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsActivityTransitionFallbackTest,
	"OpenMobile.Sensors.Activity.Transitions.Fallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsActivityTransitionFallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsActivityTransitionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ActivityTransitionFallback"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::MotionActivity),
		MakeCapability(
			EOpenMobileSensorType::ActivityTransition,
			EOpenMobileCapabilityState::NotSupported
		)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Transition =
		Snapshot.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::ActivityTransition;
			}
		);
	TestNotNull(TEXT("The transition capability is present"), Transition);
	if (Transition)
	{
		TestEqual(TEXT("Classification enables the derived transition"),
			Transition->Availability.State,
			EOpenMobileCapabilityState::Available);
		TestEqual(TEXT("The fallback source remains explicit"),
			Transition->Source,
			EOpenMobileSensorAvailabilitySource::Derived);
		TestTrue(TEXT("The fallback declares MotionActivity input"),
			Transition->Fallback.RequiredInputs.Contains(
				EOpenMobileSensorType::MotionActivity
			));
	}
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeTransitionRequest()
		);
	TestEqual(TEXT("The derived transition subscription is accepted"),
		Subscription.Operation.Code,
		EOpenMobileSensorResultCode::Accepted);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("The fallback starts classification hardware"),
		Backend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::MotionActivity);
	FOpenMobileSensorsSampleService::PublishActivity(MakeClassification(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::Medium,
		1.0
	));
	FOpenMobileSensorsSampleService::PublishActivity(MakeClassification(
		EOpenMobileMotionActivity::Running,
		EOpenMobileActivityConfidence::High,
		2.0
	));
	FOpenMobileActivitySensorBatch Batch;
	FOpenMobileSensorBufferReadResult ReadResult;
	TestTrue(TEXT("The derived transition pair is buffered"),
		FOpenMobileSensorsSampleService::DrainBufferedActivity(
			Owner,
			Subscription.Handle,
			8,
			ReadResult,
			Batch
		));
	TestEqual(TEXT("The derived pair preserves event ordering"),
		Batch.Samples.Num(), 2);
	if (Batch.Samples.Num() == 2)
	{
		TestEqual(TEXT("Stopped remains first after fanout"),
			Batch.Samples[0].Transition,
			EOpenMobileActivityTransition::Stopped);
		TestEqual(TEXT("Started remains second after fanout"),
			Batch.Samples[1].Transition,
			EOpenMobileActivityTransition::Started);
		TestEqual(TEXT("Fanout exposes the logical transition sensor"),
			Batch.Samples[1].Header.Sensor.Type,
			EOpenMobileSensorType::ActivityTransition);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeActivityTransitionPreferenceTest,
	"OpenMobile.Sensors.Activity.Transitions.NativePreference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeActivityTransitionPreferenceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsActivityTransitionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("NativeActivityTransition"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::MotionActivity),
		MakeCapability(EOpenMobileSensorType::ActivityTransition)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorSubscriptionRequest Request = MakeTransitionRequest();
	Request.Options.bAllowDerivedFallback = false;
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	TestEqual(TEXT("A native transition subscription is accepted"),
		Subscription.Operation.Code,
		EOpenMobileSensorResultCode::Accepted);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("The direct transition sensor wins"),
		Backend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::ActivityTransition);
	FOpenMobileActivitySensorBatch Published;
	Published.Samples = {
		MakeNativeTransition(
			EOpenMobileMotionActivity::Walking,
			EOpenMobileActivityTransition::Started,
			2.0
		),
		MakeNativeTransition(
			EOpenMobileMotionActivity::Running,
			EOpenMobileActivityTransition::Started,
			2.1
		),
		MakeNativeTransition(
			EOpenMobileMotionActivity::Walking,
			EOpenMobileActivityTransition::Stopped,
			2.2
		)
	};
	FOpenMobileSensorsSampleService::PublishActivityBatch(Published);
	FOpenMobileSensorsSampleService::PublishActivityBatch(Published);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileActivitySensorSample Replayed = MakeNativeTransition(
		EOpenMobileMotionActivity::Running,
		EOpenMobileActivityTransition::Started,
		3.0
	);
	Replayed.Header.bStatefulProcessingReset = true;
	FOpenMobileSensorsSampleService::PublishActivity(Replayed);
	FOpenMobileActivitySensorBatch ReadBatch;
	FOpenMobileSensorBufferReadResult ReadResult;
	TestTrue(TEXT("Native provider transitions are buffered"),
		FOpenMobileSensorsSampleService::DrainBufferedActivity(
			Owner,
			Subscription.Handle,
			8,
			ReadResult,
			ReadBatch
		));
	TestEqual(TEXT("Overlaps survive while duplicates are suppressed"),
		ReadBatch.Samples.Num(), 3);
	if (ReadBatch.Samples.Num() == 3)
	{
		TestEqual(TEXT("Native origin remains explicit"),
			ReadBatch.Samples[2].TransitionOrigin,
			EOpenMobileActivityTransitionOrigin::Native);
		TestEqual(TEXT("Native provider identity remains explicit"),
			ReadBatch.Samples[2].ActivityProvider,
			FName(TEXT("AndroidProvider")));
	}
	FinishBackend(Backend);
	return true;
}

#endif

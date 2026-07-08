#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileStepDetectionTracker.h"

namespace OpenMobileSensorsStepDetectionTestsPrivate
{
	FOpenMobileStepsSensorSample MakeDirectSample(
		double TimestampSeconds,
		int64 Delta = 1
	)
	{
		FOpenMobileStepsSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::StepDetector;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Raw
		);
		Sample.Count = Delta;
		Sample.DetectedStepDelta = Delta;
		Sample.DetectionSource =
			EOpenMobileStepDetectionSource::AndroidStepDetector;
		Sample.DetectionQuality =
			EOpenMobileStepDetectionQuality::DirectHardwareEvent;
		return Sample;
	}

	FOpenMobileStepsSensorSample MakePedometerSample(
		int64 NativeTotal,
		double TimestampSeconds,
		const FGuid& OriginIdentifier,
		double QueryStart,
		double QueryEnd,
		bool bReset = false
	)
	{
		FOpenMobileStepsSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::StepDetector;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bStatefulProcessingReset = bReset;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Raw
		);
		Sample.Count = NativeTotal;
		Sample.Origin = EOpenMobileStepCountOrigin::QueryInterval;
		Sample.OriginIdentifier = OriginIdentifier;
		Sample.bHasQueryInterval = true;
		Sample.QueryStartUnixTimeSeconds = QueryStart;
		Sample.QueryEndUnixTimeSeconds = QueryEnd;
		Sample.bHasNativeTotal = true;
		Sample.NativeTotal = NativeTotal;
		Sample.DetectionSource =
			EOpenMobileStepDetectionSource::IOSPedometerDelta;
		Sample.DetectionQuality =
			EOpenMobileStepDetectionQuality::InferredFromPedometerDelta;
		return Sample;
	}

	FOpenMobileSensorCapability MakeStepDetectorCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::StepDetector;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("StepDetector");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 10.0;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeStepDetectorRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::StepDetector;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 10.0;
		Request.Options.MaximumCallbackFrequencyHz = 10.0;
		Request.Options.DeliveryMode =
			EOpenMobileSensorDeliveryMode::EventBatches;
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
	FOpenMobileSensorsDirectStepDetectionTest,
	"OpenMobile.Sensors.Steps.Detection.Direct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDirectStepDetectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepDetectionTestsPrivate;
	const FGuid SessionIdentifier = FGuid::NewGuid();
	FOpenMobileStepDetectionTracker Tracker;
	Tracker.Initialize(SessionIdentifier);
	FOpenMobileStepsSensorSample Output;
	TestTrue(TEXT("A direct step emits an event"),
		Tracker.Process(MakeDirectSample(1.0), Output));
	TestEqual(TEXT("The first direct event has session count one"),
		Output.Count, 1LL);
	TestEqual(TEXT("The direct event represents one step"),
		Output.DetectedStepDelta, 1LL);
	TestEqual(TEXT("The session owns the output origin"),
		Output.OriginIdentifier, SessionIdentifier);
	TestFalse(TEXT("A direct event has no platform total"),
		Output.bHasNativeTotal);
	TestEqual(TEXT("The direct source remains explicit"),
		Output.DetectionSource,
		EOpenMobileStepDetectionSource::AndroidStepDetector);
	TestEqual(TEXT("The direct event has hardware quality"),
		Output.DetectionQuality,
		EOpenMobileStepDetectionQuality::DirectHardwareEvent);
	TestFalse(TEXT("A replayed direct event is suppressed"),
		Tracker.Process(MakeDirectSample(1.0), Output));
	TestTrue(TEXT("A later direct burst emits"),
		Tracker.Process(MakeDirectSample(2.0, 3), Output));
	TestEqual(TEXT("A burst advances the session count"),
		Output.Count, 4LL);
	TestEqual(TEXT("A burst preserves its delta"),
		Output.DetectedStepDelta, 3LL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPedometerStepDetectionTest,
	"OpenMobile.Sensors.Steps.Detection.PedometerDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPedometerStepDetectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepDetectionTestsPrivate;
	const FGuid Origin = FGuid::NewGuid();
	FOpenMobileStepDetectionTracker Tracker;
	Tracker.Initialize(FGuid::NewGuid());
	FOpenMobileStepsSensorSample Output;
	FOpenMobileStepsSensorSample First =
		MakePedometerSample(3, 10.0, Origin, 100.0, 110.0);
	First.Header.SourceFlags |= static_cast<int32>(
		EOpenMobileSensorSourceFlags::Replay
	);
	TestTrue(TEXT("The first pedometer total emits its delta"),
		Tracker.Process(First, Output));
	TestEqual(TEXT("The first pedometer delta becomes the session count"),
		Output.Count, 3LL);
	TestEqual(TEXT("The native interval total is preserved"),
		Output.NativeTotal, 3LL);
	TestEqual(TEXT("The first pedometer delta is explicit"),
		Output.DetectedStepDelta, 3LL);
	TestEqual(TEXT("The pedometer path is marked inferred"),
		Output.DetectionQuality,
		EOpenMobileStepDetectionQuality::InferredFromPedometerDelta);
	TestTrue(TEXT("Derived events preserve replay provenance"),
		(Output.Header.SourceFlags & static_cast<int32>(
			EOpenMobileSensorSourceFlags::Replay)) != 0);
	TestFalse(TEXT("A repeated total is suppressed"),
		Tracker.Process(
			MakePedometerSample(3, 11.0, Origin, 100.0, 111.0),
			Output));
	TestTrue(TEXT("A delayed cumulative update emits only its delta"),
		Tracker.Process(
			MakePedometerSample(5, 14.0, Origin, 100.0, 114.0),
			Output));
	TestEqual(TEXT("The cumulative update advances by two"),
		Output.DetectedStepDelta, 2LL);
	TestEqual(TEXT("The session count remains cumulative"),
		Output.Count, 5LL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStepDetectionRestartDeduplicationTest,
	"OpenMobile.Sensors.Steps.Detection.RestartDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStepDetectionRestartDeduplicationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepDetectionTestsPrivate;
	const FGuid FirstOrigin = FGuid::NewGuid();
	const FGuid OverlappingOrigin = FGuid::NewGuid();
	const FGuid ResumedOrigin = FGuid::NewGuid();
	FOpenMobileStepDetectionTracker Tracker;
	Tracker.Initialize(FGuid::NewGuid());
	FOpenMobileStepsSensorSample Output;
	TestTrue(TEXT("The initial interval emits"),
		Tracker.Process(
			MakePedometerSample(4, 10.0, FirstOrigin, 100.0, 110.0),
			Output));
	TestFalse(TEXT("An overlapping restart establishes a new baseline"),
		Tracker.Process(
			MakePedometerSample(
				6,
				12.0,
				OverlappingOrigin,
				108.0,
				112.0,
				true),
			Output));
	TestTrue(TEXT("Progress after the overlap baseline emits"),
		Tracker.Process(
			MakePedometerSample(
				8,
				14.0,
				OverlappingOrigin,
				108.0,
				114.0),
			Output));
	TestEqual(TEXT("Overlapping history is not double counted"),
		Output.Count, 6LL);
	TestFalse(TEXT("A native rollback rebases without emitting"),
		Tracker.Process(
			MakePedometerSample(
				1,
				15.0,
				OverlappingOrigin,
				108.0,
				115.0),
			Output));
	TestTrue(TEXT("Progress after rollback emits"),
		Tracker.Process(
			MakePedometerSample(
				2,
				16.0,
				OverlappingOrigin,
				108.0,
				116.0),
			Output));
	TestEqual(TEXT("Rollback keeps the confirmed session count"),
		Output.Count, 7LL);
	TestTrue(TEXT("A nonoverlapping resumed interval emits"),
		Tracker.Process(
			MakePedometerSample(
				2,
				22.0,
				ResumedOrigin,
				120.0,
				122.0,
				true),
			Output));
	TestEqual(TEXT("Pause and resume do not backfill the gap"),
		Output.Count, 9LL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStepDetectionSubscriptionTest,
	"OpenMobile.Sensors.Steps.Detection.Subscription",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStepDetectionSubscriptionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepDetectionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("StepDetection"));
	Backend.SetSensorCapabilities({MakeStepDetectorCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeStepDetectorRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("The step detector subscription starts"),
		Subscription.Operation.IsSuccess());
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	int32 CallbackCount = 0;
	FOpenMobileStepsSensorBatch ReceivedBatch;
	FOpenMobileSensorsSampleService::OnStepsBatch().AddLambda(
		[&](
			const FGuid& CallbackOwner,
			const FOpenMobileSensorSubscriptionHandle& CallbackHandle,
			const FOpenMobileStepsSensorBatch& CallbackBatch
		)
		{
			if (CallbackOwner == Owner
				&& CallbackHandle == Subscription.Handle)
			{
				++CallbackCount;
				ReceivedBatch = CallbackBatch;
			}
		}
	);
	FOpenMobileStepsSensorBatch Batch;
	Batch.Samples.Add(MakeDirectSample(1.0));
	Batch.Samples.Add(MakeDirectSample(2.0));
	TestTrue(TEXT("The native batch reaches the subscription"),
		FOpenMobileSensorsSampleService::PublishStepsBatchFromBackend(
			Token,
			PhysicalHandle,
			Batch));
	FOpenMobileSensorReadResult Read;
	FOpenMobileStepsSensorSample Latest;
	TestTrue(TEXT("The latest event is readable"),
		FOpenMobileSensorsSampleService::ReadLatestSteps(
			Owner,
			Subscription.Handle,
			0,
			2.0,
			Read,
			Latest));
	TestEqual(TEXT("Raw events become a cumulative session count"),
		Latest.Count, 2LL);
	TestEqual(TEXT("The latest event still represents one step"),
		Latest.DetectedStepDelta, 1LL);
	TestEqual(TEXT("The subscription owns the detection session"),
		Latest.OriginIdentifier,
		Subscription.Handle.GetIdentifier());
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(2.0);
	TestEqual(TEXT("The detector emits one game-thread batch"),
		CallbackCount, 1);
	TestEqual(TEXT("The event batch contains both detected steps"),
		ReceivedBatch.Samples.Num(), 2);
	if (ReceivedBatch.Samples.Num() == 2)
	{
		TestEqual(TEXT("The first callback event has session count one"),
			ReceivedBatch.Samples[0].Count, 1LL);
		TestEqual(TEXT("The second callback event has session count two"),
			ReceivedBatch.Samples[1].Count, 2LL);
	}
	FinishBackend(Backend);
	return true;
}

#endif

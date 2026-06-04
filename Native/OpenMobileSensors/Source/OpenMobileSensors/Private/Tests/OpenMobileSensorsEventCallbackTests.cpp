#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsEventCallbackTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 100.0;
		Request.Options.MaximumCallbackFrequencyHz = 10.0;
		Request.Options.DeliveryMode =
			EOpenMobileSensorDeliveryMode::EventBatches;
		return Request;
	}

	FOpenMobileVectorSensorSample MakeSample(
		double TimestampSeconds,
		double Value
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = MakeRequest().Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Value = FVector(Value, 0.0, 0.0);
		return Sample;
	}

	FOpenMobileSensorSubscriptionResult StartActive(const FGuid& Owner)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				MakeRequest()
			);
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		return Result;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsEventOffThreadBatchingTest,
	"OpenMobile.Sensors.Events.OffThreadBatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsEventOffThreadBatchingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsEventCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("OffThread"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	int32 CallbackCount = 0;
	bool bCallbackOnGameThread = false;
	FOpenMobileVectorSensorBatch ReceivedBatch;
	Subsystem->OnVectorSamplesNative().AddLambda(
		[&](
			FOpenMobileSensorSubscriptionHandle,
			const FOpenMobileVectorSensorBatch& Batch
		)
		{
			++CallbackCount;
			bCallbackOnGameThread = IsInGameThread();
			ReceivedBatch = Batch;
		}
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		Subsystem->StartSubscriptionNative(MakeRequest());
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("The subsystem subscription starts"),
		Subscription.Operation.IsSuccess());
	TFuture<void> Publisher = Async(
		EAsyncExecution::ThreadPool,
		[]()
		{
			FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.0, 1.0));
			FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.01, 2.0));
			FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.02, 3.0));
		}
	);
	Publisher.Wait();
	TestEqual(TEXT("Worker-thread publishes do not invoke delegates"),
		CallbackCount,
		0);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.02);
	TestEqual(TEXT("Pending samples are delivered as one batch"),
		CallbackCount,
		1);
	TestTrue(TEXT("The public callback runs on the game thread"),
		bCallbackOnGameThread);
	TestEqual(TEXT("All pending samples are included"),
		ReceivedBatch.Samples.Num(),
		3);
	if (ReceivedBatch.Samples.Num() == 3)
	{
		TestEqual(TEXT("Batch order preserves the first value"),
			ReceivedBatch.Samples[0].Value.X, 1.0);
		TestEqual(TEXT("Batch order preserves the last value"),
			ReceivedBatch.Samples[2].Value.X, 3.0);
		TestEqual(TEXT("Batch sequences remain monotonic"),
			ReceivedBatch.Samples[2].Header.Sequence, 3ll);
	}
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsEventCallbackThrottlingTest,
	"OpenMobile.Sensors.Events.CallbackThrottling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsEventCallbackThrottlingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsEventCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Throttle"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(Owner);
	TArray<int32> BatchSizes;
	FOpenMobileSensorsSampleService::OnVectorBatch().AddLambda(
		[&BatchSizes](
			const FGuid&,
			const FOpenMobileSensorSubscriptionHandle&,
			const FOpenMobileVectorSensorBatch& Batch
		)
		{
			BatchSizes.Add(Batch.Samples.Num());
		}
	);
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.0, 1.0));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.0);
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.01, 2.0));
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.02, 3.0));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.05);
	TestEqual(TEXT("The callback cap suppresses an early drain"),
		BatchSizes.Num(),
		1);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.1);
	TestEqual(TEXT("The next permitted callback is delivered"),
		BatchSizes.Num(),
		2);
	if (BatchSizes.Num() == 2)
	{
		TestEqual(TEXT("Samples accumulated during throttling are batched"),
			BatchSizes[1],
			2);
	}
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.03, 4.0));
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.2);
	TestEqual(TEXT("Paused streams retain pending event batches"),
		BatchSizes.Num(),
		2);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.2);
	TestEqual(TEXT("Resuming delivers the retained event batch"),
		BatchSizes.Num(),
		3);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsEventReentrantStopAndUpdateTest,
	"OpenMobile.Sensors.Events.ReentrantStopAndUpdate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsEventReentrantStopAndUpdateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsEventCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Reentrant"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(Owner);
	FOpenMobileSensorOperationResult StopResult;
	FOpenMobileSensorOperationResult UpdateResult;
	FOpenMobileSensorsSampleService::OnVectorBatch().AddLambda(
		[&](
			const FGuid&,
			const FOpenMobileSensorSubscriptionHandle&,
			const FOpenMobileVectorSensorBatch&
		)
		{
			StopResult = FOpenMobileSensorsSubscriptionService::StopSubscription(
				Owner,
				Subscription.Handle
			);
			UpdateResult =
				FOpenMobileSensorsSubscriptionService::UpdateSubscription(
					Owner,
					Subscription.Handle,
					MakeRequest().Options
				);
		}
	);
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.0, 1.0));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.0);
	TestEqual(TEXT("A callback can stop its own handle"),
		StopResult.Code,
		EOpenMobileSensorResultCode::Success);
	TestEqual(TEXT("A later reentrant update sees the stopped handle"),
		UpdateResult.Code,
		EOpenMobileSensorResultCode::InvalidHandle);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsEventOwnerDestructionTest,
	"OpenMobile.Sensors.Events.OwnerDestruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsEventOwnerDestructionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsEventCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Owner"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	int32 CallbackCount = 0;
	Subsystem->OnVectorSamplesNative().AddLambda(
		[&CallbackCount](
			FOpenMobileSensorSubscriptionHandle,
			const FOpenMobileVectorSensorBatch&
		)
		{
			++CallbackCount;
		}
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		Subsystem->StartSubscriptionNative(MakeRequest());
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.0, 1.0));
	Subsystem->Deinitialize();
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.0);
	TestEqual(TEXT("Owner teardown discards the final pending callback"),
		CallbackCount,
		0);
	TestFalse(TEXT("Owner teardown invalidates the subscription"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			Subscription.Handle
		));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsEventLateCallbackTest,
	"OpenMobile.Sensors.Events.LateCallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsEventLateCallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsEventCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Late"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(Owner);
	int32 CallbackCount = 0;
	FOpenMobileSensorsSampleService::OnVectorBatch().AddLambda(
		[&CallbackCount](
			const FGuid&,
			const FOpenMobileSensorSubscriptionHandle&,
			const FOpenMobileVectorSensorBatch&
		)
		{
			++CallbackCount;
		}
	);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Subscription.Handle
	);
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.0, 1.0));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.0);
	TestEqual(TEXT("Late publishes after stop are discarded"),
		CallbackCount,
		0);
	FinishBackend(Backend);
	return true;
}

#endif

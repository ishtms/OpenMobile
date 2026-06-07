#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsGameThreadCallbackTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorDeliveryMode DeliveryMode
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 100.0;
		Request.Options.MaximumCallbackFrequencyHz = 100.0;
		Request.Options.DeliveryMode = DeliveryMode;
		Request.Options.BufferCapacitySamples = 4096;
		return Request;
	}

	FOpenMobileVectorSensorBatch MakeBatch(
		const FOpenMobileSensorIdentifier& Sensor,
		int32 FirstValue,
		int32 Count
	)
	{
		FOpenMobileVectorSensorBatch Batch;
		Batch.Samples.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Value = FirstValue + Index;
			FOpenMobileVectorSensorSample& Sample =
				Batch.Samples.AddDefaulted_GetRef();
			Sample.Header.Sensor = Sensor;
			Sample.Header.TimestampSeconds = static_cast<double>(Value);
			Sample.Header.bValid = true;
			Sample.Value = FVector(static_cast<double>(Value), 0.0, 0.0);
		}
		return Batch;
	}

	FOpenMobileSensorSubscriptionResult StartActive(
		const FGuid& Owner,
		const FOpenMobileSensorSubscriptionRequest& Request
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				Request
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
	FOpenMobileSensorsArbitraryThreadCallbackTest,
	"OpenMobile.Sensors.ThreadCallbacks.ArbitraryThread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsArbitraryThreadCallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGameThreadCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ArbitraryThread"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorDeliveryMode::EventBatches
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	int32 CallbackCount = 0;
	bool bOnGameThread = false;
	FOpenMobileVectorSensorBatch ReceivedBatch;
	FOpenMobileSensorsSampleService::OnVectorBatch().AddLambda(
		[&](
			const FGuid&,
			const FOpenMobileSensorSubscriptionHandle&,
			const FOpenMobileVectorSensorBatch& Batch
		)
		{
			++CallbackCount;
			bOnGameThread = IsInGameThread();
			ReceivedBatch = Batch;
		}
	);
	TFuture<bool> PublishResult = Async(EAsyncExecution::ThreadPool, [&]()
	{
		return FOpenMobileSensorsSampleService::
			PublishVectorBatchFromBackend(
				Token,
				PhysicalHandle,
				MakeBatch(Request.Sensor, 1, 3)
			);
	});
	TestTrue(TEXT("An arbitrary-thread platform batch is accepted"),
		PublishResult.Get());
	TestEqual(TEXT("Native submission does not invoke public callbacks"),
		CallbackCount, 0);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(0.0);
	TestEqual(TEXT("The game-thread drain broadcasts once"),
		CallbackCount, 1);
	TestTrue(TEXT("The public event runs on the game thread"), bOnGameThread);
	TestEqual(TEXT("The platform batch stays intact"),
		ReceivedBatch.Samples.Num(), 3);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDuplicateBatchTest,
	"OpenMobile.Sensors.ThreadCallbacks.DuplicateBatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDuplicateBatchTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGameThreadCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DuplicateBatch"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorDeliveryMode::Buffered
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	const FOpenMobileVectorSensorBatch Batch = MakeBatch(Request.Sensor, 1, 3);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token, PhysicalHandle, Batch);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token, PhysicalHandle, Batch);
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch ReceivedBatch;
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		Owner, Subscription.Handle, 10, Result, ReceivedBatch);
	TestEqual(TEXT("A duplicate platform batch is ignored"),
		ReceivedBatch.Samples.Num(), 3);
	if (ReceivedBatch.Samples.Num() == 3)
	{
		TestEqual(TEXT("Duplicate rejection keeps exact sequences"),
			ReceivedBatch.Samples[2].Header.Sequence, 3ll);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStaleGenerationCallbackTest,
	"OpenMobile.Sensors.ThreadCallbacks.StaleGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStaleGenerationCallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGameThreadCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend OldBackend(TEXT("OldGeneration"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(OldBackend);
	const FOpenMobileSensorsBackendToken OldToken =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FOpenMobileSensorBackendStreamHandle OldPhysicalHandle =
		{FGuid::NewGuid()};
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(OldBackend);

	FOpenMobileSensorsMockBackend NewBackend(TEXT("NewGeneration"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(NewBackend);
	const FOpenMobileSensorsBackendToken NewToken =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorDeliveryMode::Buffered
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	const FOpenMobileSensorBackendStreamHandle NewPhysicalHandle =
		NewBackend.GetLastStartedPhysicalHandle();
	TestFalse(TEXT("A stale backend generation is rejected"),
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			OldToken,
			OldPhysicalHandle,
			MakeBatch(Request.Sensor, 1, 1)
		));
	TestFalse(TEXT("A stale physical stream is rejected"),
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			NewToken,
			OldPhysicalHandle,
			MakeBatch(Request.Sensor, 2, 1)
		));
	TestTrue(TEXT("The current generation and stream are accepted"),
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			NewToken,
			NewPhysicalHandle,
			MakeBatch(Request.Sensor, 3, 1)
		));
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch ReceivedBatch;
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		Owner, Subscription.Handle, 10, Result, ReceivedBatch);
	TestEqual(TEXT("Only the current platform sample is stored"),
		ReceivedBatch.Samples.Num(), 1);
	if (ReceivedBatch.Samples.Num() == 1)
	{
		TestEqual(TEXT("The accepted sample has the current value"),
			ReceivedBatch.Samples[0].Value.X, 3.0);
	}
	FinishBackend(NewBackend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCallbackControlRacesTest,
	"OpenMobile.Sensors.ThreadCallbacks.ControlRaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCallbackControlRacesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGameThreadCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ControlRaces"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorDeliveryMode::Buffered
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	TAtomic<bool> bStart(false);
	TFuture<void> Publisher = Async(EAsyncExecution::ThreadPool, [&]()
	{
		while (!bStart.Load())
		{
			FPlatformProcess::YieldThread();
		}
		for (int32 FirstValue = 1; FirstValue <= 1000; FirstValue += 20)
		{
			FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
				Token,
				PhysicalHandle,
				MakeBatch(Request.Sensor, FirstValue, 20)
			);
		}
	});
	bStart.Store(true);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		FOpenMobileSensorsSampleService::SetSubscriptionState(
			Subscription.Handle,
			Index % 2 == 0
				? EOpenMobileSensorSubscriptionState::Paused
				: EOpenMobileSensorSubscriptionState::Active
		);
		FOpenMobileSensorStreamOptions UpdatedOptions = Request.Options;
		UpdatedOptions.MaximumCallbackFrequencyHz =
			Index % 2 == 0 ? 30.0 : 60.0;
		FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
			Subscription.Handle,
			UpdatedOptions
		);
	}
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	Publisher.Wait();
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch ReceivedBatch;
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		Owner, Subscription.Handle, 4096, Result, ReceivedBatch);
	bool bMonotonic = true;
	for (int32 Index = 1; Index < ReceivedBatch.Samples.Num(); ++Index)
	{
		bMonotonic &= ReceivedBatch.Samples[Index].Header.TimestampSeconds
			> ReceivedBatch.Samples[Index - 1].Header.TimestampSeconds;
	}
	TestTrue(TEXT("Control races preserve strict stream order"), bMonotonic);
	TestTrue(TEXT("Control races remain memory bounded"),
		Result.BufferHighWaterMark <= 4096);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShutdownAndOwnerTeardownTest,
	"OpenMobile.Sensors.ThreadCallbacks.ShutdownAndOwnerTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShutdownAndOwnerTeardownTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGameThreadCallbackTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ShutdownRace"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorDeliveryMode::EventBatches
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		Subsystem->StartSubscriptionNative(Request);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	TAtomic<bool> bStop(false);
	TAtomic<int32> RejectedPublishes(0);
	TFuture<void> Publisher = Async(EAsyncExecution::ThreadPool, [&]()
	{
		int32 Value = 1;
		while (!bStop.Load())
		{
			if (!FOpenMobileSensorsSampleService::
				PublishVectorBatchFromBackend(
					Token,
					PhysicalHandle,
					MakeBatch(Request.Sensor, Value++, 1)
				))
			{
				RejectedPublishes++;
			}
		}
	});
	Subsystem->Deinitialize();
	FOpenMobileSensorsSampleService::BeginShutdown();
	for (int32 Index = 0; Index < 100; ++Index)
	{
		FPlatformProcess::YieldThread();
	}
	bStop.Store(true);
	Publisher.Wait();
	TestTrue(TEXT("Shutdown rejects racing platform callbacks"),
		RejectedPublishes.Load() > 0);
	TestFalse(TEXT("Owner teardown leaves the handle invalid"),
		FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
			Subscription.Handle
		));
	FOpenMobileSensorsSampleService::ResetForTests();
	FinishBackend(Backend);
	return true;
}

#endif

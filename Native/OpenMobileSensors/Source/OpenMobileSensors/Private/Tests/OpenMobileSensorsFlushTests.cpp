#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsFlushTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::Buffered
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.MaximumCallbackFrequencyHz = 60.0;
		Request.Options.MaximumDeliveryLatencySeconds = 0.1;
		Request.Options.DeliveryMode = DeliveryMode;
		Request.Options.BufferCapacitySamples = 8;
		return Request;
	}

	FOpenMobileSensorCapability MakeCapability(bool bNativeBatching)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("Accelerometer");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 200.0;
		Capability.bSupportsNativeBatching = bNativeBatching;
		return Capability;
	}

	FOpenMobileVectorSensorBatch MakeBatch(
		const FOpenMobileSensorIdentifier& Sensor,
		double FirstTimestamp,
		int32 SampleCount
	)
	{
		FOpenMobileVectorSensorBatch Batch;
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			FOpenMobileVectorSensorSample Sample;
			Sample.Header.Sensor = Sensor;
			Sample.Header.TimestampSeconds = FirstTimestamp + Index * 0.01;
			Sample.Header.bValid = true;
			Batch.Samples.Add(Sample);
		}
		return Batch;
	}

	UOpenMobileSensorsSubsystem* MakeSubsystem()
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		return NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	}

	FOpenMobileSensorSubscriptionResult StartActive(
		UOpenMobileSensorsSubsystem& Subsystem,
		const FOpenMobileSensorSubscriptionRequest& Request
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			Subsystem.StartSubscriptionNative(Request);
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		return Result;
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

	FOpenMobileSensorOperationResult MakeSuccess()
	{
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::Success;
		return Result;
	}

	FOpenMobileSensorOperationResult MakeAccepted()
	{
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::Accepted;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsFlushEmptyAndPopulatedTest,
	"OpenMobile.Sensors.Flush.EmptyAndPopulated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsFlushEmptyAndPopulatedTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsFlushTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PluginFlush"));
	Backend.SetSensorCapabilities({MakeCapability(false)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UOpenMobileSensorsSubsystem* Subsystem = MakeSubsystem();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(*Subsystem, Request);
	FOpenMobileSensorsSampleService::PublishVectorBatch(
		MakeBatch(Request.Sensor, 1.0, 3)
	);
	int32 CompletionCount = 0;
	FOpenMobileSensorFlushResult PopulatedResult;
	const FGuid PopulatedRequestId = Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++CompletionCount;
				PopulatedResult = Result;
			}
		)
	);
	TestTrue(TEXT("A flush returns a request identifier"),
		PopulatedRequestId.IsValid());
	TestEqual(TEXT("A populated plugin flush completes once"),
		CompletionCount, 1);
	TestEqual(TEXT("The completion preserves its request identifier"),
		PopulatedResult.RequestId, PopulatedRequestId);
	TestTrue(TEXT("The completion exposes a typed flush handle"),
		PopulatedResult.Request.IsValid());
	TestEqual(TEXT("The typed flush handle preserves its request"),
		PopulatedResult.Request.GetIdentifier(), PopulatedRequestId);
	TestEqual(TEXT("The flush reports queued plugin samples"),
		PopulatedResult.FlushedSamples, 3);
	FOpenMobileSensorBufferReadResult ReadResult;
	FOpenMobileVectorSensorBatch ReadBatch;
	Subsystem->GetBufferedVectorSamplesNative(
		Subscription.Handle,
		8,
		ReadResult,
		ReadBatch
	);
	TestEqual(TEXT("Flush does not consume the public buffer"),
		ReadBatch.Samples.Num(), 3);
	FOpenMobileSensorFlushResult EmptyResult;
	Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++CompletionCount;
				EmptyResult = Result;
			}
		)
	);
	TestEqual(TEXT("An empty flush also completes exactly once"),
		CompletionCount, 2);
	TestTrue(TEXT("An empty flush succeeds"),
		EmptyResult.Operation.IsSuccess());
	TestEqual(TEXT("An empty flush reports no samples"),
		EmptyResult.FlushedSamples, 0);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsFlushUnsupportedTest,
	"OpenMobile.Sensors.Flush.Unsupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsFlushUnsupportedTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsFlushTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("UnsupportedFlush"));
	Backend.SetSensorCapabilities({MakeCapability(true)});
	Backend.SetNativeBatchingAppliedForTests(true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UOpenMobileSensorsSubsystem* Subsystem = MakeSubsystem();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(*Subsystem,
			MakeRequest(EOpenMobileSensorDeliveryMode::LatestValue));
	int32 CompletionCount = 0;
	FOpenMobileSensorFlushResult FlushResult;
	Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++CompletionCount;
				FlushResult = Result;
			}
		)
	);
	TestEqual(TEXT("Unsupported flush completes once"), CompletionCount, 1);
	TestEqual(TEXT("Unsupported flush stays distinct"),
		FlushResult.Operation.Code,
		EOpenMobileSensorResultCode::NotSupported);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsConcurrentFlushTest,
	"OpenMobile.Sensors.Flush.Concurrent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsConcurrentFlushTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsFlushTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ConcurrentFlush"));
	Backend.SetSensorCapabilities({MakeCapability(true)});
	Backend.SetNativeBatchingAppliedForTests(true);
	Backend.SetFlushSensorStreamResult(MakeAccepted());
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	UOpenMobileSensorsSubsystem* Subsystem = MakeSubsystem();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(*Subsystem, Request);
	int32 FirstCount = 0;
	int32 SecondCount = 0;
	FOpenMobileSensorFlushResult FirstResult;
	const FGuid FirstRequestId = Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++FirstCount;
				FirstResult = Result;
			}
		)
	);
	FOpenMobileSensorFlushResult SecondResult;
	Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++SecondCount;
				SecondResult = Result;
			}
		)
	);
	TestEqual(TEXT("The first native flush remains pending"), FirstCount, 0);
	TestEqual(TEXT("A concurrent physical flush fails once"), SecondCount, 1);
	TestEqual(TEXT("A concurrent flush is a failed operation"),
		SecondResult.Operation.Code,
		EOpenMobileSensorResultCode::Failed);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		Backend.GetLastStartedPhysicalHandle(),
		MakeBatch(Request.Sensor, 1.0, 2)
	);
	Backend.CompleteFlushForTests(FirstRequestId, MakeSuccess());
	TestEqual(TEXT("The accepted flush later completes once"), FirstCount, 1);
	TestEqual(TEXT("Native FIFO samples reach the plugin buffer first"),
		FirstResult.FlushedSamples, 2);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTimedOutFlushTest,
	"OpenMobile.Sensors.Flush.TimedOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTimedOutFlushTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsFlushTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("TimedOutFlush"));
	Backend.SetSensorCapabilities({MakeCapability(true)});
	Backend.SetNativeBatchingAppliedForTests(true);
	Backend.SetFlushSensorStreamResult(MakeAccepted());
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UOpenMobileSensorsSubsystem* Subsystem = MakeSubsystem();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(*Subsystem, MakeRequest());
	int32 CompletionCount = 0;
	FOpenMobileSensorFlushResult FlushResult;
	const FGuid RequestId = Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++CompletionCount;
				FlushResult = Result;
			}
		)
	);
	FOpenMobileSensorsSubscriptionService::ProcessFlushTimeoutsForTests(
		FPlatformTime::Seconds() + 10.0
	);
	TestEqual(TEXT("A timed-out flush completes exactly once"),
		CompletionCount, 1);
	TestEqual(TEXT("A timeout is reported as failed"),
		FlushResult.Operation.Code,
		EOpenMobileSensorResultCode::Failed);
	Backend.CompleteFlushForTests(RequestId, MakeSuccess());
	TestEqual(TEXT("A late callback after timeout is ignored"),
		CompletionCount, 1);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLateNativeFlushCallbacksTest,
	"OpenMobile.Sensors.Flush.LateNativeCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLateNativeFlushCallbacksTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsFlushTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("LateFlush"));
	Backend.SetSensorCapabilities({MakeCapability(true)});
	Backend.SetNativeBatchingAppliedForTests(true);
	Backend.SetFlushSensorStreamResult(MakeAccepted());
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UOpenMobileSensorsSubsystem* Subsystem = MakeSubsystem();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(*Subsystem, MakeRequest());
	int32 CompletionCount = 0;
	FOpenMobileSensorFlushResult FlushResult;
	const FGuid RequestId = Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++CompletionCount;
				FlushResult = Result;
			}
		)
	);
	Subsystem->StopSubscriptionNative(Subscription.Handle);
	TestEqual(TEXT("Stop cancels an accepted flush once"),
		CompletionCount, 1);
	TestEqual(TEXT("Stop reports cancellation"),
		FlushResult.Operation.Code,
		EOpenMobileSensorResultCode::Cancelled);
	Backend.CompleteFlushForTests(RequestId, MakeSuccess());
	TestEqual(TEXT("A late native completion is ignored"),
		CompletionCount, 1);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsFlushStopAndRateChangeTest,
	"OpenMobile.Sensors.Flush.StopAndRateChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsFlushStopAndRateChangeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsFlushTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("FlushRateChange"));
	Backend.SetSensorCapabilities({MakeCapability(true)});
	Backend.SetNativeBatchingAppliedForTests(true);
	Backend.SetFlushSensorStreamResult(MakeAccepted());
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UOpenMobileSensorsSubsystem* Subsystem = MakeSubsystem();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(*Subsystem, Request);
	int32 CompletionCount = 0;
	FOpenMobileSensorFlushResult FlushResult;
	const FGuid RequestId = Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++CompletionCount;
				FlushResult = Result;
			}
		)
	);
	FOpenMobileSensorStreamOptions Updated = Request.Options;
	Updated.CustomFrequencyHz = 30.0;
	Subsystem->UpdateSubscriptionNative(Subscription.Handle, Updated);
	TestEqual(TEXT("Rate change cancels the pending flush once"),
		CompletionCount, 1);
	TestEqual(TEXT("Rate change reports cancellation"),
		FlushResult.Operation.Code,
		EOpenMobileSensorResultCode::Cancelled);
	Backend.CompleteFlushForTests(RequestId, MakeSuccess());
	TestEqual(TEXT("The old-rate completion stays ignored"),
		CompletionCount, 1);
	FOpenMobileSensorFlushResult PauseResult;
	const FGuid PauseRequestId = Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++CompletionCount;
				PauseResult = Result;
			}
		)
	);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	TestEqual(TEXT("Pause cancels the next pending flush once"),
		CompletionCount, 2);
	TestEqual(TEXT("Pause reports cancellation"),
		PauseResult.Operation.Code,
		EOpenMobileSensorResultCode::Cancelled);
	Backend.CompleteFlushForTests(PauseRequestId, MakeSuccess());
	TestEqual(TEXT("The pre-pause completion stays ignored"),
		CompletionCount, 2);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileSensorFlushResult ShutdownResult;
	const FGuid ShutdownRequestId = Subsystem->FlushNative(
		Subscription.Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&](const FOpenMobileSensorFlushResult& Result)
			{
				++CompletionCount;
				ShutdownResult = Result;
			}
		)
	);
	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	TestEqual(TEXT("Shutdown cancels the next pending flush once"),
		CompletionCount, 3);
	TestEqual(TEXT("Shutdown reports cancellation"),
		ShutdownResult.Operation.Code,
		EOpenMobileSensorResultCode::Cancelled);
	Backend.CompleteFlushForTests(ShutdownRequestId, MakeSuccess());
	TestEqual(TEXT("The pre-shutdown completion stays ignored"),
		CompletionCount, 3);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

#endif

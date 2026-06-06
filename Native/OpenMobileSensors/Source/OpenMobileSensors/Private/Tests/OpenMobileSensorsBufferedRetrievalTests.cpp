#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsBufferedRetrievalTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorType SensorType = EOpenMobileSensorType::Accelerometer,
		int32 Capacity = 8,
		EOpenMobileSensorOverflowPolicy OverflowPolicy =
			EOpenMobileSensorOverflowPolicy::DropOldest
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = SensorType;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 100.0;
		Request.Options.DeliveryMode = EOpenMobileSensorDeliveryMode::Buffered;
		Request.Options.BufferCapacitySamples = Capacity;
		Request.Options.OverflowPolicy = OverflowPolicy;
		return Request;
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

	FOpenMobileVectorSensorSample MakeVectorSample(
		const FOpenMobileSensorIdentifier& Sensor,
		double TimestampSeconds,
		double Value
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Value = FVector(Value, 0.0, 0.0);
		return Sample;
	}

	FOpenMobileVectorSensorBatch MakeVectorBatch(
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
			Batch.Samples.Add(MakeVectorSample(
				Sensor,
				static_cast<double>(Value),
				static_cast<double>(Value)
			));
		}
		return Batch;
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
	FOpenMobileSensorsOrderedBoundedDrainTest,
	"OpenMobile.Sensors.Buffered.OrderedBoundedDrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsOrderedBoundedDrainTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBufferedRetrievalTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("OrderedBuffer"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		Subsystem->StartSubscriptionNative(Request);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("A bounded native batch is accepted"),
		FOpenMobileSensorsSampleService::PublishVectorBatch(
			MakeVectorBatch(Request.Sensor, 1, 5)
		));
	FOpenMobileVectorSensorBatch OversizedBatch;
	OversizedBatch.Samples.SetNum(4097);
	TestFalse(TEXT("An oversized native batch is rejected"),
		FOpenMobileSensorsSampleService::PublishVectorBatch(OversizedBatch));

	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch Batch;
	TestTrue(TEXT("The first bounded drain succeeds"),
		Subsystem->GetBufferedVectorSamplesNative(
			Subscription.Handle, 2, Result, Batch));
	TestEqual(TEXT("The first drain is bounded"), Batch.Samples.Num(), 2);
	TestEqual(TEXT("The high-water mark records the full queue"),
		Result.BufferHighWaterMark, 5);
	if (Batch.Samples.Num() == 2)
	{
		TestEqual(TEXT("The first drain starts with the oldest value"),
			Batch.Samples[0].Value.X, 1.0);
		TestEqual(TEXT("Logical sequences begin at one"),
			Batch.Samples[0].Header.Sequence, 1ll);
		TestEqual(TEXT("The first drain remains ordered"),
			Batch.Samples[1].Header.Sequence, 2ll);
	}
	TestTrue(TEXT("The second drain succeeds"),
		Subsystem->GetBufferedVectorSamplesNative(
			Subscription.Handle, 8, Result, Batch));
	TestEqual(TEXT("The second drain returns the remainder"),
		Batch.Samples.Num(), 3);
	if (Batch.Samples.Num() == 3)
	{
		TestEqual(TEXT("The second drain continues the sequence"),
			Batch.Samples[0].Header.Sequence, 3ll);
		TestEqual(TEXT("The final value remains ordered"),
			Batch.Samples[2].Value.X, 5.0);
	}
	TestFalse(TEXT("A zero-sized drain is rejected"),
		Subsystem->GetBufferedVectorSamplesNative(
			Subscription.Handle, 0, Result, Batch));
	TestEqual(TEXT("A zero-sized drain reports an invalid argument"),
		Result.Operation.Code, EOpenMobileSensorResultCode::InvalidArgument);
	TestFalse(TEXT("An oversized drain is rejected"),
		Subsystem->GetBufferedVectorSamplesNative(
			Subscription.Handle, 4097, Result, Batch));
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsOverflowPoliciesAndWraparoundTest,
	"OpenMobile.Sensors.Buffered.OverflowPoliciesAndWraparound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsOverflowPoliciesAndWraparoundTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBufferedRetrievalTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Overflow"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid DropOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest DropRequest = MakeRequest(
		EOpenMobileSensorType::Accelerometer,
		3,
		EOpenMobileSensorOverflowPolicy::DropOldest
	);
	const FOpenMobileSensorSubscriptionResult DropSubscription =
		StartActive(DropOwner, DropRequest);
	FOpenMobileSensorsSampleService::PublishVectorBatch(
		MakeVectorBatch(DropRequest.Sensor, 1, 3)
	);
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch Batch;
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		DropOwner, DropSubscription.Handle, 2, Result, Batch);
	FOpenMobileSensorsSampleService::PublishVectorBatch(
		MakeVectorBatch(DropRequest.Sensor, 4, 3)
	);
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		DropOwner, DropSubscription.Handle, 3, Result, Batch);
	TestEqual(TEXT("Drop-oldest wraparound keeps capacity"),
		Batch.Samples.Num(), 3);
	TestEqual(TEXT("Drop-oldest records overwritten samples"),
		Result.DroppedSamples, 1ll);
	TestEqual(TEXT("Drop-oldest records bounded high-water"),
		Result.BufferHighWaterMark, 3);
	if (Batch.Samples.Num() == 3)
	{
		TestEqual(TEXT("Wraparound drops the remaining oldest value"),
			Batch.Samples[0].Value.X, 4.0);
		TestEqual(TEXT("Wraparound keeps the newest value"),
			Batch.Samples[2].Value.X, 6.0);
	}

	const FGuid RejectOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest RejectRequest = MakeRequest(
		EOpenMobileSensorType::Gyroscope,
		3,
		EOpenMobileSensorOverflowPolicy::RejectNewest
	);
	const FOpenMobileSensorSubscriptionResult RejectSubscription =
		StartActive(RejectOwner, RejectRequest);
	FOpenMobileSensorsSampleService::PublishVectorBatch(
		MakeVectorBatch(RejectRequest.Sensor, 1, 5)
	);
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		RejectOwner, RejectSubscription.Handle, 3, Result, Batch);
	TestEqual(TEXT("Reject-newest reports rejected samples"),
		Result.DroppedSamples, 2ll);
	if (Batch.Samples.Num() == 3)
	{
		TestEqual(TEXT("Reject-newest preserves the oldest value"),
			Batch.Samples[0].Value.X, 1.0);
		TestEqual(TEXT("Reject-newest preserves the third value"),
			Batch.Samples[2].Value.X, 3.0);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBufferPauseAndTeardownTest,
	"OpenMobile.Sensors.Buffered.PauseAndTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBufferPauseAndTeardownTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBufferedRetrievalTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("BufferLifecycle"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	FOpenMobileSensorsSampleService::PublishVectorBatch(
		MakeVectorBatch(Request.Sensor, 1, 2)
	);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeVectorSample(Request.Sensor, 3.0, 3.0)
	);
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch Batch;
	TestTrue(TEXT("Paused buffers remain readable"),
		FOpenMobileSensorsSampleService::DrainBufferedVector(
			Owner, Subscription.Handle, 8, Result, Batch));
	TestEqual(TEXT("Pause retains accepted samples and rejects new input"),
		Batch.Samples.Num(), 2);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Subscription.Handle
	);
	TestFalse(TEXT("Stopped buffers are invalidated"),
		FOpenMobileSensorsSampleService::DrainBufferedVector(
			Owner, Subscription.Handle, 8, Result, Batch));
	TestEqual(TEXT("Stopped buffers report an invalid handle"),
		Result.Operation.Code, EOpenMobileSensorResultCode::InvalidHandle);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsConcurrentBufferDrainTest,
	"OpenMobile.Sensors.Buffered.ConcurrentDrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsConcurrentBufferDrainTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBufferedRetrievalTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ConcurrentBuffer"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorType::Accelerometer,
		64,
		EOpenMobileSensorOverflowPolicy::RejectNewest
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	TAtomic<bool> bStart(false);
	TAtomic<bool> bDone(false);
	TArray<int64> ReceivedSequences;
	int64 DroppedSamples = 0;
	int32 HighWaterMark = 0;
	TFuture<void> Publisher = Async(EAsyncExecution::ThreadPool, [&]()
	{
		while (!bStart.Load())
		{
			FPlatformProcess::YieldThread();
		}
		for (int32 FirstValue = 1; FirstValue <= 2000; FirstValue += 25)
		{
			FOpenMobileSensorsSampleService::PublishVectorBatch(
				MakeVectorBatch(Request.Sensor, FirstValue, 25)
			);
		}
		bDone.Store(true);
	});
	TFuture<void> Drainer = Async(EAsyncExecution::ThreadPool, [&]()
	{
		while (!bStart.Load())
		{
			FPlatformProcess::YieldThread();
		}
		FOpenMobileSensorBufferReadResult Result;
		FOpenMobileVectorSensorBatch Batch;
		do
		{
			FOpenMobileSensorsSampleService::DrainBufferedVector(
				Owner, Subscription.Handle, 17, Result, Batch);
			for (const FOpenMobileVectorSensorSample& Sample : Batch.Samples)
			{
				ReceivedSequences.Add(Sample.Header.Sequence);
			}
			DroppedSamples = Result.DroppedSamples;
			HighWaterMark = Result.BufferHighWaterMark;
		} while (!bDone.Load() || !Batch.Samples.IsEmpty());
	});
	bStart.Store(true);
	Publisher.Wait();
	Drainer.Wait();
	bool bMonotonic = true;
	for (int32 Index = 1; Index < ReceivedSequences.Num(); ++Index)
	{
		bMonotonic &= ReceivedSequences[Index] > ReceivedSequences[Index - 1];
	}
	TestTrue(TEXT("Concurrent drains preserve sequence order"), bMonotonic);
	TestEqual(TEXT("Every published sample is returned or rejected"),
		static_cast<int64>(ReceivedSequences.Num()) + DroppedSamples,
		2000ll);
	TestTrue(TEXT("Concurrent buffering stays within capacity"),
		HighWaterMark <= 64);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBufferedAllSampleFamiliesTest,
	"OpenMobile.Sensors.Buffered.AllSampleFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBufferedAllSampleFamiliesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBufferedRetrievalTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("BufferFamilies"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorBufferReadResult Result;

#define OPENMOBILE_TEST_BUFFER_FAMILY( \
	SensorType, SampleType, BatchType, PublishMethod, DrainMethod \
) \
	{ \
		const FOpenMobileSensorSubscriptionRequest Request = MakeRequest( \
			EOpenMobileSensorType::SensorType); \
		const FOpenMobileSensorSubscriptionResult Subscription = \
			StartActive(Owner, Request); \
		SampleType Sample; \
		Sample.Header.Sensor = Request.Sensor; \
		Sample.Header.TimestampSeconds = 1.0; \
		Sample.Header.bValid = true; \
		FOpenMobileSensorsSampleService::PublishMethod(Sample); \
		BatchType Batch; \
		TestTrue(TEXT(#SensorType " buffer drains"), \
			FOpenMobileSensorsSampleService::DrainMethod( \
				Owner, Subscription.Handle, 1, Result, Batch)); \
		TestEqual(TEXT(#SensorType " returns one sample"), \
			Batch.Samples.Num(), 1); \
	}

	OPENMOBILE_TEST_BUFFER_FAMILY(
		Accelerometer,
		FOpenMobileVectorSensorSample,
		FOpenMobileVectorSensorBatch,
		PublishVector,
		DrainBufferedVector
	)
	OPENMOBILE_TEST_BUFFER_FAMILY(
		Attitude,
		FOpenMobileAttitudeSensorSample,
		FOpenMobileAttitudeSensorBatch,
		PublishAttitude,
		DrainBufferedAttitude
	)
	OPENMOBILE_TEST_BUFFER_FAMILY(
		BarometricPressure,
		FOpenMobileScalarSensorSample,
		FOpenMobileScalarSensorBatch,
		PublishScalar,
		DrainBufferedScalar
	)
	OPENMOBILE_TEST_BUFFER_FAMILY(
		MagneticHeading,
		FOpenMobileHeadingSensorSample,
		FOpenMobileHeadingSensorBatch,
		PublishHeading,
		DrainBufferedHeading
	)
	OPENMOBILE_TEST_BUFFER_FAMILY(
		StepCounter,
		FOpenMobileStepsSensorSample,
		FOpenMobileStepsSensorBatch,
		PublishSteps,
		DrainBufferedSteps
	)
	OPENMOBILE_TEST_BUFFER_FAMILY(
		MotionActivity,
		FOpenMobileActivitySensorSample,
		FOpenMobileActivitySensorBatch,
		PublishActivity,
		DrainBufferedActivity
	)
	OPENMOBILE_TEST_BUFFER_FAMILY(
		PhysicalOrientation,
		FOpenMobileOrientationSensorSample,
		FOpenMobileOrientationSensorBatch,
		PublishOrientation,
		DrainBufferedOrientation
	)
	OPENMOBILE_TEST_BUFFER_FAMILY(
		Proximity,
		FOpenMobileProximitySensorSample,
		FOpenMobileProximitySensorBatch,
		PublishProximity,
		DrainBufferedProximity
	)

#undef OPENMOBILE_TEST_BUFFER_FAMILY

	FinishBackend(Backend);
	return true;
}

#endif

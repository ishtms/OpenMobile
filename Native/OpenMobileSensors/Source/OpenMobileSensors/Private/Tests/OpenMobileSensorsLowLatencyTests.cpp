#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsLowLatencyTestsPrivate
{
	FOpenMobileSensorCapability MakeCapability(EOpenMobileSensorType Type)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 200.0;
		Capability.bSupportsNativeBatching = true;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorType Type,
		double FrequencyHz,
		double CallbackFrequencyHz,
		bool bLowLatency,
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::Buffered,
		int32 BufferCapacity = 8
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = Type;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = FrequencyHz;
		Request.Options.MaximumDeliveryLatencySeconds = 0.5;
		Request.Options.MaximumCallbackFrequencyHz = CallbackFrequencyHz;
		Request.Options.DeliveryMode = DeliveryMode;
		Request.Options.BufferCapacitySamples = BufferCapacity;
		Request.Options.bLowLatency = bLowLatency;
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
			Sample.Header.TimestampSeconds = FirstTimestamp + Index * 0.1;
			Sample.Header.bValid = true;
			Batch.Samples.Add(Sample);
		}
		return Batch;
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

	const FOpenMobileSensorStreamDiagnostics* FindDiagnostics(
		const FOpenMobileSensorDiagnosticsSnapshot& Snapshot,
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		return Snapshot.Streams.FindByPredicate(
			[&Handle](const FOpenMobileSensorStreamDiagnostics& Stream)
			{
				return Stream.Subscription.Handle == Handle;
			}
		);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLowLatencyExplicitModeTest,
	"OpenMobile.Sensors.LowLatency.ExplicitMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLowLatencyExplicitModeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLowLatencyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ExplicitLowLatency"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	Backend.SetNativeBatchingAppliedForTests(true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		MakeRequest(EOpenMobileSensorType::Accelerometer,
			200.0, 15.0, true, EOpenMobileSensorDeliveryMode::Buffered, 4)
	);
	TestTrue(TEXT("The low-latency request is accepted"),
		Subscription.Operation.IsSuccess());
	TestTrue(TEXT("The applied options retain explicit low-latency mode"),
		Subscription.AppliedOptions.bLowLatency);
	TestEqual(TEXT("The applied delivery latency is immediate"),
		Subscription.AppliedOptions.MaximumDeliveryLatencySeconds, 0.0);
	TestEqual(TEXT("The physical stream retains the requested sample rate"),
		Backend.GetLastStartedPhysicalRequest().RequestedFrequencyHz, 200.0);
	TestFalse(TEXT("The physical stream does not request native batching"),
		Backend.GetLastStartedPhysicalRequest().bNativeBatchingRequested);
	TestEqual(TEXT("The queue remains explicitly bounded"),
		Subscription.AppliedOptions.BufferCapacitySamples, 4);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLowLatencyPhysicalStreamIsolationTest,
	"OpenMobile.Sensors.LowLatency.PhysicalStreamIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLowLatencyPhysicalStreamIsolationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLowLatencyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("LowLatencyIsolation"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::Gyroscope)
	});
	Backend.SetNativeBatchingAppliedForTests(true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	const FOpenMobileSensorSubscriptionResult LowLatency =
		Subsystem->StartSubscriptionNative(MakeRequest(
			EOpenMobileSensorType::Accelerometer, 120.0, 30.0, true));
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorSubscriptionResult Batched =
		Subsystem->StartSubscriptionNative(MakeRequest(
			EOpenMobileSensorType::Gyroscope, 60.0, 30.0, false));
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorDiagnosticsSnapshot Diagnostics =
		Subsystem->GetDiagnosticsSnapshotNative();
	const FOpenMobileSensorStreamDiagnostics* LowLatencyDiagnostics =
		FindDiagnostics(Diagnostics, LowLatency.Handle);
	const FOpenMobileSensorStreamDiagnostics* BatchedDiagnostics =
		FindDiagnostics(Diagnostics, Batched.Handle);
	TestNotNull(TEXT("The low-latency stream has diagnostics"),
		LowLatencyDiagnostics);
	TestNotNull(TEXT("The batched stream has diagnostics"),
		BatchedDiagnostics);
	if (LowLatencyDiagnostics && BatchedDiagnostics)
	{
		TestEqual(TEXT("Only the low-latency stream disables batching"),
			LowLatencyDiagnostics->BatchingMode,
			EOpenMobileSensorBatchingMode::Disabled);
		TestEqual(TEXT("The other physical stream keeps native batching"),
			BatchedDiagnostics->BatchingMode,
			EOpenMobileSensorBatchingMode::Native);
	}
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLowLatencyCallbackCapTest,
	"OpenMobile.Sensors.LowLatency.CallbackCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLowLatencyCallbackCapTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLowLatencyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("LowLatencyCallbackCap"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorType::Accelerometer,
		120.0,
		20.0,
		true,
		EOpenMobileSensorDeliveryMode::EventBatches
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	TestTrue(TEXT("The first logical delivery is due"),
		FOpenMobileSensorsSubscriptionService::SelectSubscribersForSample(
			Request.Sensor, 1.0).Contains(Subscription.Handle));
	TestFalse(TEXT("The callback cap rejects an early delivery"),
		FOpenMobileSensorsSubscriptionService::SelectSubscribersForSample(
			Request.Sensor, 1.01).Contains(Subscription.Handle));
	TestTrue(TEXT("The callback cap permits the next 20 Hz delivery"),
		FOpenMobileSensorsSubscriptionService::SelectSubscribersForSample(
			Request.Sensor, 1.05).Contains(Subscription.Handle));
	TestEqual(TEXT("The native stream still runs at 120 Hz"),
		Backend.GetLastStartedPhysicalRequest().RequestedFrequencyHz, 120.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLowLatencyDevelopmentDiagnosticsTest,
	"OpenMobile.Sensors.LowLatency.DevelopmentDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLowLatencyDevelopmentDiagnosticsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLowLatencyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("LowLatencyDiagnostics"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorType::Accelerometer,
		60.0,
		30.0,
		true,
		EOpenMobileSensorDeliveryMode::Buffered,
		3
	);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	FOpenMobileSensorsSampleService::PublishVectorBatch(
		MakeBatch(Request.Sensor, 10.0, 5)
	);
	FOpenMobileSensorStreamDiagnostics Diagnostics;
	TestTrue(TEXT("Development delivery diagnostics are available"),
		FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
			Owner,
			Subscription.Handle,
			11.0,
			Diagnostics
		));
	TestEqual(TEXT("Diagnostics report bounded queue depth"),
		Diagnostics.QueueDepth, 3);
	TestEqual(TEXT("Diagnostics report the queue high-water mark"),
		Diagnostics.BufferHighWaterMark, 3);
	TestEqual(TEXT("Diagnostics report dropped low-latency samples"),
		Diagnostics.DroppedSamples, 2ll);
	TestTrue(TEXT("Diagnostics report latest sample age"),
		FMath::IsNearlyEqual(Diagnostics.LatestSampleAgeSeconds, 0.6));
	TestTrue(TEXT("Diagnostics report oldest queued delay"),
		FMath::IsNearlyEqual(Diagnostics.QueueDelaySeconds, 0.8));
	TestTrue(TEXT("Game-thread processing cost stays finite"),
		FMath::IsFinite(Diagnostics.GameThreadProcessingSeconds)
			&& Diagnostics.GameThreadProcessingSeconds >= 0.0);
	FinishBackend(Backend);
	return true;
}

#endif

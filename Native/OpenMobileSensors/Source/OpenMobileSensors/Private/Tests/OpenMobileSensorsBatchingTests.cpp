#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsBatchingTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		double FrequencyHz,
		double DeliveryLatencySeconds,
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::Buffered,
		int32 BufferCapacity = 8,
		bool bLowLatency = false
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = FrequencyHz;
		Request.Options.MaximumDeliveryLatencySeconds =
			DeliveryLatencySeconds;
		Request.Options.MaximumCallbackFrequencyHz =
			FMath::Min(FrequencyHz, 120.0);
		Request.Options.DeliveryMode = DeliveryMode;
		Request.Options.BufferCapacitySamples = BufferCapacity;
		Request.Options.bLowLatency = bLowLatency;
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
			Sample.Header.TimestampSeconds = FirstTimestamp + Index * 0.01;
			Sample.Header.bValid = true;
			Sample.Value = FVector3d(Index, 0.0, 0.0);
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
	FOpenMobileSensorsMixedLatencySubscribersTest,
	"OpenMobile.Sensors.Batching.MixedLatencySubscribers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMixedLatencySubscribersTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBatchingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("MixedLatency"));
	Backend.SetSensorCapabilities({MakeCapability(true)});
	Backend.SetNativeBatchingAppliedForTests(true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Slow = StartActive(
		Owner,
		MakeRequest(30.0, 0.5)
	);
	const FOpenMobileSensorSubscriptionResult Fast = StartActive(
		Owner,
		MakeRequest(60.0, 0.02)
	);
	const FOpenMobileSensorPhysicalStreamRequest& Shared =
		Backend.GetLastReconfiguredPhysicalRequest();
	TestEqual(TEXT("The shared stream uses the highest sample frequency"),
		Shared.RequestedFrequencyHz, 60.0);
	TestEqual(TEXT("The shared stream honors the lowest delivery latency"),
		Shared.MaximumDeliveryLatencySeconds, 0.02);
	TestTrue(TEXT("Native batching is requested independently of frequency"),
		Shared.bNativeBatchingRequested);
	FOpenMobileSensorsSubscriptionService::StopSubscription(Owner, Fast.Handle);
	TestEqual(TEXT("Removing the low-latency subscriber restores batching"),
		Backend.GetLastReconfiguredPhysicalRequest()
			.MaximumDeliveryLatencySeconds,
		0.5);
	FOpenMobileSensorsSubscriptionService::StopSubscription(Owner, Slow.Handle);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeAndFallbackDiagnosticsTest,
	"OpenMobile.Sensors.Batching.NativeAndFallbackDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeAndFallbackDiagnosticsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBatchingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("BatchDiagnostics"));
	Backend.SetSensorCapabilities({MakeCapability(true)});
	Backend.SetNativeBatchingAppliedForTests(true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	const FOpenMobileSensorSubscriptionResult Native =
		Subsystem->StartSubscriptionNative(MakeRequest(60.0, 0.2));
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorDiagnosticsSnapshot NativeSnapshot =
		Subsystem->GetDiagnosticsSnapshotNative();
	const FOpenMobileSensorStreamDiagnostics* NativeDiagnostics =
		FindDiagnostics(NativeSnapshot, Native.Handle);
	TestNotNull(TEXT("The native stream has diagnostics"), NativeDiagnostics);
	if (NativeDiagnostics)
	{
		TestEqual(TEXT("Applied native batching is reported"),
			NativeDiagnostics->BatchingMode,
			EOpenMobileSensorBatchingMode::Native);
	}

	Backend.SetNativeBatchingAppliedForTests(false);
	const FOpenMobileSensorSubscriptionResult Plugin =
		Subsystem->StartSubscriptionNative(MakeRequest(30.0, 0.1));
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorSubscriptionResult LowLatency =
		Subsystem->StartSubscriptionNative(MakeRequest(20.0, 0.5,
			EOpenMobileSensorDeliveryMode::Buffered, 8, true));
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Low-latency sharing requests immediate delivery"),
		Backend.GetLastReconfiguredPhysicalRequest()
			.MaximumDeliveryLatencySeconds,
		0.0);
	TestFalse(TEXT("Low-latency sharing disables native batching"),
		Backend.GetLastReconfiguredPhysicalRequest()
			.bNativeBatchingRequested);
	const FOpenMobileSensorSubscriptionResult Unavailable =
		Subsystem->StartSubscriptionNative(MakeRequest(15.0, 0.1,
			EOpenMobileSensorDeliveryMode::LatestValue));
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorDiagnosticsSnapshot FallbackSnapshot =
		Subsystem->GetDiagnosticsSnapshotNative();
	const FOpenMobileSensorStreamDiagnostics* PluginDiagnostics =
		FindDiagnostics(FallbackSnapshot, Plugin.Handle);
	const FOpenMobileSensorStreamDiagnostics* LowLatencyDiagnostics =
		FindDiagnostics(FallbackSnapshot, LowLatency.Handle);
	const FOpenMobileSensorStreamDiagnostics* UnavailableDiagnostics =
		FindDiagnostics(FallbackSnapshot, Unavailable.Handle);
	TestNotNull(TEXT("The plugin stream has diagnostics"), PluginDiagnostics);
	TestNotNull(TEXT("The low-latency stream has diagnostics"),
		LowLatencyDiagnostics);
	TestNotNull(TEXT("The latest-value stream has diagnostics"),
		UnavailableDiagnostics);
	if (PluginDiagnostics && LowLatencyDiagnostics && UnavailableDiagnostics)
	{
		TestEqual(TEXT("The bounded fallback is reported"),
			PluginDiagnostics->BatchingMode,
			EOpenMobileSensorBatchingMode::Plugin);
		TestEqual(TEXT("Low-latency mode disables batching"),
			LowLatencyDiagnostics->BatchingMode,
			EOpenMobileSensorBatchingMode::Disabled);
		TestEqual(TEXT("A latest-value fallback reports unavailable"),
			UnavailableDiagnostics->BatchingMode,
			EOpenMobileSensorBatchingMode::Unavailable);
	}
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPartialNativeBatchesTest,
	"OpenMobile.Sensors.Batching.PartialNativeBatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPartialNativeBatchesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBatchingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PartialBatches"));
	Backend.SetSensorCapabilities({MakeCapability(true)});
	Backend.SetNativeBatchingAppliedForTests(true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request =
		MakeRequest(60.0, 0.1);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		PhysicalHandle,
		MakeBatch(Request.Sensor, 1.0, 2)
	);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		PhysicalHandle,
		MakeBatch(Request.Sensor, 1.02, 2)
	);
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch Received;
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		Owner,
		Subscription.Handle,
		8,
		Result,
		Received
	);
	TestEqual(TEXT("Partial native batches share one ordered buffer"),
		Received.Samples.Num(), 4);
	if (Received.Samples.Num() == 4)
	{
		TestEqual(TEXT("Partial batches retain logical ordering"),
			Received.Samples[3].Header.Sequence, 4ll);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPluginBufferOverflowTest,
	"OpenMobile.Sensors.Batching.PluginBufferOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPluginBufferOverflowTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBatchingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PluginOverflow"));
	Backend.SetSensorCapabilities({MakeCapability(false)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request =
		MakeRequest(60.0, 0.1, EOpenMobileSensorDeliveryMode::Buffered, 3);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		Backend.GetLastStartedPhysicalHandle(),
		MakeBatch(Request.Sensor, 1.0, 5)
	);
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch Received;
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		Owner,
		Subscription.Handle,
		3,
		Result,
		Received
	);
	TestEqual(TEXT("The plugin fallback remains bounded"),
		Received.Samples.Num(), 3);
	TestEqual(TEXT("Overflow reports dropped native-batch samples"),
		Result.DroppedSamples, 2ll);
	if (Received.Samples.Num() == 3)
	{
		TestEqual(TEXT("Drop-oldest keeps the newest batch sequence"),
			Received.Samples[0].Header.Sequence, 3ll);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBatchingForegroundTransitionsTest,
	"OpenMobile.Sensors.Batching.ForegroundTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBatchingForegroundTransitionsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsBatchingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("BatchForeground"));
	Backend.SetSensorCapabilities({MakeCapability(false)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request =
		MakeRequest(60.0, 0.1);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		PhysicalHandle,
		MakeBatch(Request.Sensor, 1.0, 2)
	);
	FOpenMobileSensorsCapabilityService::SetApplicationActive(false);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		PhysicalHandle,
		MakeBatch(Request.Sensor, 2.0, 2)
	);
	FOpenMobileSensorsCapabilityService::SetApplicationActive(true);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		PhysicalHandle,
		MakeBatch(Request.Sensor, 3.0, 2)
	);
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch Received;
	FOpenMobileSensorsSampleService::DrainBufferedVector(
		Owner,
		Subscription.Handle,
		8,
		Result,
		Received
	);
	TestEqual(TEXT("Foreground resume preserves accepted buffered samples"),
		Received.Samples.Num(), 4);
	if (Received.Samples.Num() == 4)
	{
		TestEqual(TEXT("Suspended callbacks do not enter the buffer"),
			Received.Samples[2].Header.TimestampSeconds, 3.0);
	}
	FinishBackend(Backend);
	return true;
}

#endif

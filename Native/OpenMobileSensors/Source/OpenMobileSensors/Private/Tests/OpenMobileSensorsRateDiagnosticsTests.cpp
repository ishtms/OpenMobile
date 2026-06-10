#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsRateDiagnosticsTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(double FrequencyHz = 100.0)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = FrequencyHz;
		return Request;
	}

	FOpenMobileVectorSensorSample MakeSample(
		const FOpenMobileSensorIdentifier& Sensor,
		double TimestampSeconds
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		return Sample;
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

	FOpenMobileSensorRateDiagnostics ReadRate(
		const FGuid& Owner,
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		FOpenMobileSensorRateDiagnostics Rate;
		FOpenMobileSensorsSampleService::GetRateDiagnostics(
			Owner,
			Handle,
			Rate
		);
		return Rate;
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
	FOpenMobileSensorsSteadyRateWindowTest,
	"OpenMobile.Sensors.RateDiagnostics.SteadyWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSteadyRateWindowTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRateDiagnosticsTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("SteadyRate"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	for (int32 Index = 0; Index < 80; ++Index)
	{
		FOpenMobileSensorsSampleService::PublishVector(
			MakeSample(Request.Sensor, 1.0 + Index * 0.01)
		);
	}
	const FOpenMobileSensorRateDiagnostics Rate = ReadRate(
		Owner,
		Subscription.Handle
	);
	TestEqual(TEXT("The rolling window stays bounded"), Rate.SampleCount, 65);
	TestTrue(TEXT("Steady input reports 100 Hz"),
		FMath::IsNearlyEqual(Rate.MeanFrequencyHz, 100.0, 1.e-8));
	TestTrue(TEXT("Steady input has negligible jitter"),
		Rate.IntervalJitterSeconds < 1.e-10);
	TestTrue(TEXT("The final sample gap is reported"),
		FMath::IsNearlyEqual(Rate.LastGapSeconds, 0.01, 1.e-10));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsJitteredRateIntervalsTest,
	"OpenMobile.Sensors.RateDiagnostics.JitteredIntervals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsJitteredRateIntervalsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRateDiagnosticsTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("JitteredRate"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	for (double Timestamp : {1.0, 1.01, 1.03, 1.04, 1.06})
	{
		FOpenMobileSensorsSampleService::PublishVector(
			MakeSample(Request.Sensor, Timestamp)
		);
	}
	const FOpenMobileSensorRateDiagnostics Rate = ReadRate(
		Owner,
		Subscription.Handle
	);
	TestTrue(TEXT("Jittered input uses timestamp intervals"),
		FMath::IsNearlyEqual(
			Rate.MeanFrequencyHz,
			1.0 / 0.015,
			1.e-8
		));
	TestTrue(TEXT("Jitter is the interval standard deviation"),
		FMath::IsNearlyEqual(
			Rate.IntervalJitterSeconds,
			0.005,
			1.e-10
		));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBurstBatchedRateTest,
	"OpenMobile.Sensors.RateDiagnostics.BurstBatched",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBurstBatchedRateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRateDiagnosticsTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("BurstRate"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	FOpenMobileVectorSensorBatch Batch;
	for (int32 Index = 0; Index < 11; ++Index)
	{
		Batch.Samples.Add(MakeSample(Request.Sensor, 1.0 + Index * 0.01));
	}
	FOpenMobileSensorsSampleService::PublishVectorBatch(Batch);
	const FOpenMobileSensorRateDiagnostics Rate = ReadRate(
		Owner,
		Subscription.Handle
	);
	TestEqual(TEXT("The batch contributes every timestamp"),
		Rate.SampleCount, 11);
	TestTrue(TEXT("Batch delivery rate does not replace sample rate"),
		FMath::IsNearlyEqual(Rate.MeanFrequencyHz, 100.0, 1.e-8));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStallClockResetAndRestartTest,
	"OpenMobile.Sensors.RateDiagnostics.StallClockResetAndRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStallClockResetAndRestartTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRateDiagnosticsTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RateReset"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeSample(Request.Sensor, 1.0));
	FOpenMobileSensorsSampleService::PublishVector(
		MakeSample(Request.Sensor, 1.01));
	FOpenMobileSensorsSampleService::PublishVector(
		MakeSample(Request.Sensor, 10.0));
	FOpenMobileSensorRateDiagnostics Rate = ReadRate(
		Owner,
		Subscription.Handle
	);
	TestEqual(TEXT("A long stall starts a new rate window"),
		Rate.SampleCount, 1);
	TestEqual(TEXT("A reset window has no mean yet"),
		Rate.MeanFrequencyHz, 0.0);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeSample(Request.Sensor, 9.0));
	Rate = ReadRate(Owner, Subscription.Handle);
	TestEqual(TEXT("A backward clock resets rate statistics"),
		Rate.SampleCount, 0);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Subscription.Handle
	);
	const FOpenMobileSensorSubscriptionResult Restarted =
		StartActive(Owner, Request);
	Rate = ReadRate(Owner, Restarted.Handle);
	TestEqual(TEXT("A restarted stream has an empty window"),
		Rate.SampleCount, 0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRequestedAndAppliedRateTest,
	"OpenMobile.Sensors.RateDiagnostics.RequestedAndApplied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRequestedAndAppliedRateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRateDiagnosticsTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ReportedRates"));
	Backend.SetAppliedStartFrequencyForTests(50.0);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	const FOpenMobileSensorSubscriptionResult Subscription =
		Subsystem->StartSubscriptionNative(MakeRequest(80.0));
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorDiagnosticsSnapshot Diagnostics =
		Subsystem->GetDiagnosticsSnapshotNative();
	TestEqual(TEXT("One owned stream is reported"),
		Diagnostics.Streams.Num(), 1);
	if (Diagnostics.Streams.Num() == 1)
	{
		TestEqual(TEXT("Diagnostics report requested frequency"),
			Diagnostics.Streams[0].Rate.RequestedFrequencyHz, 80.0);
		TestEqual(TEXT("Diagnostics report backend-applied frequency"),
			Diagnostics.Streams[0].Rate.AppliedFrequencyHz, 50.0);
		TestEqual(TEXT("Diagnostics identify the owned handle"),
			Diagnostics.Streams[0].Subscription.Handle,
			Subscription.Handle);
	}
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

#endif

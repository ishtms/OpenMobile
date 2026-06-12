#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorTimestamp.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

#include <limits>

namespace OpenMobileSensorsTimestampTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 100.0;
		Request.Options.MaximumCallbackFrequencyHz = 100.0;
		Request.Options.DeliveryMode = DeliveryMode;
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

	FOpenMobileSensorSubscriptionResult StartActive(
		const FGuid& Owner,
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				MakeRequest(DeliveryMode)
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
	FOpenMobileSensorsTimestampConversionPrecisionTest,
	"OpenMobile.Sensors.Timestamp.ConversionPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTimestampConversionPrecisionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const double AndroidSeconds = FOpenMobileSensorTimestampConverter::
		FromAndroidSensorEventNanoseconds(1234567890123456ll);
	TestTrue(TEXT("Android nanoseconds retain sub-microsecond precision"),
		FMath::IsNearlyEqual(AndroidSeconds, 1234567.890123456, 1.e-9));
	const double IOSSeconds = 1234567.890123456;
	TestEqual(TEXT("Core Motion seconds stay on their native timeline"),
		FOpenMobileSensorTimestampConverter::FromIOSCoreMotionSeconds(
			IOSSeconds), IOSSeconds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTimestampLargeUptimeTest,
	"OpenMobile.Sensors.Timestamp.LargeUptime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTimestampLargeUptimeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const double Converted = FOpenMobileSensorTimestampConverter::
		FromAndroidSensorEventNanoseconds(15552000000123456ll);
	TestTrue(TEXT("A 180-day uptime retains microsecond precision"),
		FMath::IsNearlyEqual(Converted, 15552000.000123456, 2.e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTimestampBatchSequenceAndReceiptTest,
	"OpenMobile.Sensors.Timestamp.BatchSequenceAndReceipt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTimestampBatchSequenceAndReceiptTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsTimestampTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("TimestampBatch"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		EOpenMobileSensorDeliveryMode::EventBatches
	);
	FOpenMobileVectorSensorBatch Received;
	const FDelegateHandle Delegate =
		FOpenMobileSensorsSampleService::OnVectorBatch().AddLambda(
			[&](
				const FGuid& EventOwner,
				const FOpenMobileSensorSubscriptionHandle& EventHandle,
				const FOpenMobileVectorSensorBatch& Batch
			)
			{
				if (EventOwner == Owner && EventHandle == Subscription.Handle)
				{
					Received = Batch;
				}
			}
		);
	FOpenMobileVectorSensorBatch Batch;
	Batch.Samples = {
		MakeSample(15552000.0, 1.0),
		MakeSample(15552000.01, 2.0),
		MakeSample(15552000.02, 3.0)
	};
	FOpenMobileSensorsSampleService::PublishVectorBatch(Batch);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(15552000.5);
	TestEqual(TEXT("The native batch remains one callback"),
		Received.Samples.Num(), 3);
	for (int32 Index = 0; Index < Received.Samples.Num(); ++Index)
	{
		const FOpenMobileSensorSampleHeader& Header =
			Received.Samples[Index].Header;
		TestEqual(TEXT("Batch sequences preserve native order"),
			Header.Sequence, static_cast<int64>(Index + 1));
		TestTrue(TEXT("Game-thread receipt time is marked"),
			Header.bHasGameThreadReceiptTime);
		TestEqual(TEXT("One drain shares one receipt time"),
			Header.GameThreadReceiptSeconds, 15552000.5);
	}
	FOpenMobileSensorsSampleService::OnVectorBatch().Remove(Delegate);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTimestampInvalidDuplicateBackwardTest,
	"OpenMobile.Sensors.Timestamp.InvalidDuplicateBackward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTimestampInvalidDuplicateBackwardTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsTimestampTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("TimestampIssues"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(Owner);
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(10.0, 1.0));
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(10.0, 2.0));
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(9.0, 3.0));
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(
		std::numeric_limits<double>::quiet_NaN(), 4.0));
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(11.0, 5.0));
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Sample;
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner, Subscription.Handle, 0, 11.0, Read, Sample);
	const int32 ExpectedIssues =
		static_cast<int32>(EOpenMobileSensorTimestampIssue::Invalid)
		| static_cast<int32>(EOpenMobileSensorTimestampIssue::Duplicate)
		| static_cast<int32>(EOpenMobileSensorTimestampIssue::Backward);
	TestEqual(TEXT("Rejected timestamps do not replace valid samples"),
		Sample.Value.X, 5.0);
	TestEqual(TEXT("Only accepted samples advance the sequence"),
		Sample.Header.Sequence, 2ll);
	TestEqual(TEXT("The next accepted sample marks every issue"),
		Sample.Header.TimestampIssueFlags, ExpectedIssues);
	TestTrue(TEXT("Timestamp issues reset stateful processing"),
		Sample.Header.bStatefulProcessingReset);
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(12.0, 6.0));
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner, Subscription.Handle, 0, 12.0, Read, Sample);
	TestEqual(TEXT("The reset marker is consumed once"),
		Sample.Header.TimestampIssueFlags, 0);
	TestFalse(TEXT("Later monotonic samples do not reset state"),
		Sample.Header.bStatefulProcessingReset);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTimestampWallClockIndependenceTest,
	"OpenMobile.Sensors.Timestamp.WallClockIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTimestampWallClockIndependenceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	constexpr int64 NativeTimestampNanoseconds = 5000000123ll;
	const double BeforeWallClockChange = FOpenMobileSensorTimestampConverter::
		FromAndroidSensorEventNanoseconds(NativeTimestampNanoseconds);
	const double AfterWallClockChange = FOpenMobileSensorTimestampConverter::
		FromAndroidSensorEventNanoseconds(NativeTimestampNanoseconds);
	TestEqual(TEXT("Wall-clock changes cannot alter a native timestamp"),
		AfterWallClockChange, BeforeWallClockChange);
	return true;
}

#endif

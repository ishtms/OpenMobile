#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsLatestValueTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorType Type
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = Type;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.MaximumCallbackFrequencyHz = 30.0;
		return Request;
	}

	FOpenMobileSensorSubscriptionResult StartActive(
		const FGuid& Owner,
		EOpenMobileSensorType Type
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				MakeRequest(Type)
			);
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		return Result;
	}

	FOpenMobileSensorCapability MakeAttitudeCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeRequest(EOpenMobileSensorType::Attitude).Sensor;
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		return Capability;
	}

	FOpenMobileVectorSensorSample MakeVector(
		const FOpenMobileSensorIdentifier& Sensor,
		double TimestampSeconds,
		double Value,
		bool bValid = true
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = bValid;
		Sample.Header.Accuracy = EOpenMobileSensorAccuracy::High;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Raw
		);
		Sample.Value = FVector(Value, 0.0, 0.0);
		return Sample;
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
	FOpenMobileSensorsLatestBeforeFirstSampleTest,
	"OpenMobile.Sensors.Latest.BeforeFirstSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLatestBeforeFirstSampleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLatestValueTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("BeforeFirst"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		EOpenMobileSensorType::Accelerometer
	);
	FOpenMobileSensorReadResult ReadResult;
	FOpenMobileVectorSensorSample Sample;
	TestFalse(TEXT("No sample is returned before the first publish"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			0,
			1.0,
			ReadResult,
			Sample
		));
	TestEqual(TEXT("The read status distinguishes no sample"),
		ReadResult.Status,
		EOpenMobileSensorReadStatus::NoSample);
	TestEqual(TEXT("The output sample is reset"),
		Sample.Value,
		FVector::ZeroVector);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLatestExactSequenceTransitionsTest,
	"OpenMobile.Sensors.Latest.ExactSequenceTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLatestExactSequenceTransitionsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLatestValueTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Sequences"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		EOpenMobileSensorType::Accelerometer
	);
	FOpenMobileSensorsSampleService::PublishVector(MakeVector(
		MakeRequest(EOpenMobileSensorType::Accelerometer).Sensor,
		10.0,
		1.0
	));
	FOpenMobileSensorsSampleService::PublishVector(MakeVector(
		MakeRequest(EOpenMobileSensorType::Accelerometer).Sensor,
		10.1,
		2.0
	));
	FOpenMobileSensorReadResult ReadResult;
	FOpenMobileVectorSensorSample Sample;
	TestTrue(TEXT("The latest valid sample is returned"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			0,
			10.2,
			ReadResult,
			Sample
		));
	TestEqual(TEXT("Logical sequences advance exactly once per publish"),
		ReadResult.Sequence,
		2ll);
	TestEqual(TEXT("The newest value replaces the prior snapshot"),
		Sample.Value.X,
		2.0);
	TestTrue(TEXT("A higher sequence is reported as newer"),
		ReadResult.bHasNewerSample);
	TestTrue(TEXT("Validity is copied into the read result"),
		ReadResult.bSampleValid);
	TestEqual(TEXT("Accuracy is copied into the read result"),
		ReadResult.Accuracy,
		EOpenMobileSensorAccuracy::High);
	TestEqual(TEXT("Source flags are copied into the read result"),
		ReadResult.SourceFlags,
		static_cast<int32>(EOpenMobileSensorSourceFlags::Raw));
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner,
		Subscription.Handle,
		2,
		10.2,
		ReadResult,
		Sample
	);
	TestFalse(TEXT("An exact last-seen sequence is not newer"),
		ReadResult.bHasNewerSample);
	FOpenMobileSensorsSampleService::PublishVector(MakeVector(
		MakeRequest(EOpenMobileSensorType::Accelerometer).Sensor,
		10.3,
		99.0,
		false
	));
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner,
		Subscription.Handle,
		2,
		10.4,
		ReadResult,
		Sample
	);
	TestEqual(TEXT("Invalid publishes do not advance the sequence"),
		ReadResult.Sequence,
		2ll);
	TestEqual(TEXT("Invalid publishes do not replace the snapshot"),
		Sample.Value.X,
		2.0);
	FOpenMobileSensorsSampleService::PublishVector(MakeVector(
		MakeRequest(EOpenMobileSensorType::Accelerometer).Sensor,
		9.9,
		100.0
	));
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner,
		Subscription.Handle,
		2,
		10.4,
		ReadResult,
		Sample
	);
	TestEqual(TEXT("Out-of-order publishes do not advance the sequence"),
		ReadResult.Sequence,
		2ll);
	TestEqual(TEXT("Out-of-order publishes do not replace the latest value"),
		Sample.Value.X,
		2.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLatestStaleAndPausedTest,
	"OpenMobile.Sensors.Latest.StaleAndPaused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLatestStaleAndPausedTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLatestValueTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("StalePaused"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		EOpenMobileSensorType::Accelerometer
	);
	FOpenMobileSensorsSampleService::PublishVector(MakeVector(
		MakeRequest(EOpenMobileSensorType::Accelerometer).Sensor,
		5.0,
		4.0
	));
	FOpenMobileSensorReadResult ReadResult;
	FOpenMobileVectorSensorSample Sample;
	TestTrue(TEXT("Stale snapshots remain readable"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			0,
			7.0,
			ReadResult,
			Sample
		));
	TestEqual(TEXT("Old samples are marked stale"),
		ReadResult.Status,
		EOpenMobileSensorReadStatus::Stale);
	TestEqual(TEXT("Sample age uses monotonic timestamps"),
		ReadResult.SampleAgeSeconds,
		2.0);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	TestTrue(TEXT("A paused stream preserves its latest snapshot"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			0,
			7.0,
			ReadResult,
			Sample
		));
	TestEqual(TEXT("Paused reads remain explicitly paused"),
		ReadResult.Status,
		EOpenMobileSensorReadStatus::Paused);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLatestStoppedAndPermissionLossTest,
	"OpenMobile.Sensors.Latest.StoppedAndPermissionLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLatestStoppedAndPermissionLossTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLatestValueTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Stopped"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Stopped = StartActive(
		Owner,
		EOpenMobileSensorType::Accelerometer
	);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Stopped.Handle
	);
	FOpenMobileSensorReadResult ReadResult;
	FOpenMobileVectorSensorSample Sample;
	TestFalse(TEXT("Stopped handles cannot read cached samples"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Stopped.Handle,
			0,
			1.0,
			ReadResult,
			Sample
		));
	TestEqual(TEXT("Stopped handles are invalid after teardown"),
		ReadResult.Status,
		EOpenMobileSensorReadStatus::InvalidHandle);
	const FOpenMobileSensorSubscriptionResult PermissionLost = StartActive(
		Owner,
		EOpenMobileSensorType::MotionActivity
	);
	FOpenMobileSensorsSubscriptionService::
		InvalidateForUnrecoverablePermissionLoss(
			EOpenMobileSensorType::MotionActivity
		);
	TestFalse(TEXT("Permission loss removes the latest-value slot"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			PermissionLost.Handle,
			0,
			1.0,
			ReadResult,
			Sample
		));
	TestEqual(TEXT("Permission-lost handles are invalid"),
		ReadResult.Status,
		EOpenMobileSensorReadStatus::InvalidHandle);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLatestAllSampleFamiliesTest,
	"OpenMobile.Sensors.Latest.AllSampleFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLatestAllSampleFamiliesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLatestValueTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Families"));
	FOpenMobileSensorCapability PhysicalOrientation = MakeAttitudeCapability();
	PhysicalOrientation.Sensor =
		MakeRequest(EOpenMobileSensorType::PhysicalOrientation).Sensor;
	FOpenMobileSensorCapability Pressure = MakeAttitudeCapability();
	Pressure.Sensor =
		MakeRequest(EOpenMobileSensorType::BarometricPressure).Sensor;
	FOpenMobileSensorCapability ProximityCapability =
		MakeAttitudeCapability();
	ProximityCapability.Sensor =
		MakeRequest(EOpenMobileSensorType::Proximity).Sensor;
	FOpenMobileSensorCapability StepCounterCapability =
		MakeAttitudeCapability();
	StepCounterCapability.Sensor =
		MakeRequest(EOpenMobileSensorType::StepCounter).Sensor;
	Backend.SetSensorCapabilities({
		MakeAttitudeCapability(),
		PhysicalOrientation,
		Pressure,
		ProximityCapability,
		StepCounterCapability
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Vector = StartActive(
		Owner, EOpenMobileSensorType::Accelerometer);
	const FOpenMobileSensorSubscriptionResult Attitude = StartActive(
		Owner, EOpenMobileSensorType::Attitude);
	const FOpenMobileSensorSubscriptionResult Scalar = StartActive(
		Owner, EOpenMobileSensorType::BarometricPressure);
	const FOpenMobileSensorSubscriptionResult Heading = StartActive(
		Owner, EOpenMobileSensorType::MagneticHeading);
	const FOpenMobileSensorSubscriptionResult Steps = StartActive(
		Owner, EOpenMobileSensorType::StepCounter);
	const FOpenMobileSensorSubscriptionResult Activity = StartActive(
		Owner, EOpenMobileSensorType::MotionActivity);
	const FOpenMobileSensorSubscriptionResult Orientation = StartActive(
		Owner, EOpenMobileSensorType::PhysicalOrientation);
	const FOpenMobileSensorSubscriptionResult Proximity = StartActive(
		Owner, EOpenMobileSensorType::Proximity);

	FOpenMobileVectorSensorSample VectorSample = MakeVector(
		MakeRequest(EOpenMobileSensorType::Accelerometer).Sensor, 1.0, 1.0);
	FOpenMobileAttitudeSensorSample AttitudeSample;
	AttitudeSample.Header.Sensor =
		MakeRequest(EOpenMobileSensorType::Attitude).Sensor;
	AttitudeSample.Header.TimestampSeconds = 1.0;
	AttitudeSample.Header.bValid = true;
	FOpenMobileScalarSensorSample ScalarSample;
	ScalarSample.Header.Sensor =
		MakeRequest(EOpenMobileSensorType::BarometricPressure).Sensor;
	ScalarSample.Header.TimestampSeconds = 1.0;
	ScalarSample.Header.bValid = true;
	ScalarSample.Value = 1013.25;
	FOpenMobileHeadingSensorSample HeadingSample;
	HeadingSample.Header.Sensor =
		MakeRequest(EOpenMobileSensorType::MagneticHeading).Sensor;
	HeadingSample.Header.TimestampSeconds = 1.0;
	HeadingSample.Header.bValid = true;
	FOpenMobileStepsSensorSample StepsSample;
	StepsSample.Header.Sensor =
		MakeRequest(EOpenMobileSensorType::StepCounter).Sensor;
	StepsSample.Header.TimestampSeconds = 1.0;
	StepsSample.Header.bValid = true;
	FOpenMobileActivitySensorSample ActivitySample;
	ActivitySample.Header.Sensor =
		MakeRequest(EOpenMobileSensorType::MotionActivity).Sensor;
	ActivitySample.Header.TimestampSeconds = 1.0;
	ActivitySample.Header.bValid = true;
	FOpenMobileOrientationSensorSample OrientationSample;
	OrientationSample.Header.Sensor =
		MakeRequest(EOpenMobileSensorType::PhysicalOrientation).Sensor;
	OrientationSample.Header.TimestampSeconds = 1.0;
	OrientationSample.Header.bValid = true;
	FOpenMobileProximitySensorSample ProximitySample;
	ProximitySample.Header.Sensor =
		MakeRequest(EOpenMobileSensorType::Proximity).Sensor;
	ProximitySample.Header.TimestampSeconds = 1.0;
	ProximitySample.Header.bValid = true;
	FOpenMobileSensorsSampleService::PublishVector(VectorSample);
	FOpenMobileSensorsSampleService::PublishAttitude(AttitudeSample);
	FOpenMobileSensorsSampleService::PublishScalar(ScalarSample);
	FOpenMobileSensorsSampleService::PublishHeading(HeadingSample);
	FOpenMobileSensorsSampleService::PublishSteps(StepsSample);
	FOpenMobileSensorsSampleService::PublishActivity(ActivitySample);
	FOpenMobileSensorsSampleService::PublishOrientation(OrientationSample);
	FOpenMobileSensorsSampleService::PublishProximity(ProximitySample);

	FOpenMobileSensorReadResult ReadResult;
	TestTrue(TEXT("Vector latest reads are supported"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner, Vector.Handle, 0, 1.1, ReadResult, VectorSample));
	TestTrue(TEXT("Attitude latest reads are supported"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner, Attitude.Handle, 0, 1.1, ReadResult, AttitudeSample));
	TestTrue(TEXT("Scalar latest reads are supported"),
		FOpenMobileSensorsSampleService::ReadLatestScalar(
			Owner, Scalar.Handle, 0, 1.1, ReadResult, ScalarSample));
	TestTrue(TEXT("Heading latest reads are supported"),
		FOpenMobileSensorsSampleService::ReadLatestHeading(
			Owner, Heading.Handle, 0, 1.1, ReadResult, HeadingSample));
	TestTrue(TEXT("Steps latest reads are supported"),
		FOpenMobileSensorsSampleService::ReadLatestSteps(
			Owner, Steps.Handle, 0, 1.1, ReadResult, StepsSample));
	TestTrue(TEXT("Activity latest reads are supported"),
		FOpenMobileSensorsSampleService::ReadLatestActivity(
			Owner, Activity.Handle, 0, 1.1, ReadResult, ActivitySample));
	TestTrue(TEXT("Orientation latest reads are supported"),
		FOpenMobileSensorsSampleService::ReadLatestOrientation(
			Owner, Orientation.Handle, 0, 1.1, ReadResult, OrientationSample));
	TestTrue(TEXT("Proximity latest reads are supported"),
		FOpenMobileSensorsSampleService::ReadLatestProximity(
			Owner, Proximity.Handle, 0, 1.1, ReadResult, ProximitySample));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLatestConcurrentPublishAndReadTest,
	"OpenMobile.Sensors.Latest.ConcurrentPublishAndRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLatestConcurrentPublishAndReadTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLatestValueTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Concurrent"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		EOpenMobileSensorType::Accelerometer
	);
	const FOpenMobileSensorIdentifier Sensor =
		MakeRequest(EOpenMobileSensorType::Accelerometer).Sensor;
	TFuture<void> Publisher = Async(
		EAsyncExecution::ThreadPool,
		[Sensor]()
		{
			for (int32 Index = 0; Index < 1000; ++Index)
			{
				FOpenMobileSensorsSampleService::PublishVector(
					MakeVector(Sensor, 1.0 + Index * 0.001, Index)
				);
			}
		}
	);
	FOpenMobileSensorReadResult ReadResult;
	FOpenMobileVectorSensorSample Sample;
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			ReadResult.Sequence,
			2.0,
			ReadResult,
			Sample
		);
	}
	Publisher.Wait();
	TestTrue(TEXT("The final concurrent snapshot is readable"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			0,
			2.0,
			ReadResult,
			Sample
		));
	TestEqual(TEXT("Concurrent publishes preserve exact sequencing"),
		ReadResult.Sequence,
		1000ll);
	TestEqual(TEXT("The final concurrent value is intact"),
		Sample.Value.X,
		999.0);
	FinishBackend(Backend);
	return true;
}

#endif

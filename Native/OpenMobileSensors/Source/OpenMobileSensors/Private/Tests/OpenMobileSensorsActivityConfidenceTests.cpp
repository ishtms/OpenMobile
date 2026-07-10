#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"
#include "OpenMobileActivitySampleFilter.h"
#include "OpenMobileMotionActivityClassifier.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsActivityConfidenceTestsPrivate
{
	FOpenMobileActivitySensorSample MakeSample(
		EOpenMobileMotionActivity Activity,
		EOpenMobileActivityConfidence Confidence,
		double TimestampSeconds
	)
	{
		FOpenMobileActivitySensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::MotionActivity;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Activity = Activity;
		Sample.Confidence = Confidence;
		Sample.ConcurrentActivities = {Activity};
		return Sample;
	}

	FOpenMobileSensorCapability MakeCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::MotionActivity;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("MotionActivity");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileActivityConfidence MinimumConfidence,
		double MinimumStableDurationSeconds
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::MotionActivity;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.MinimumActivityConfidence = MinimumConfidence;
		Request.Options.MinimumActivityStableDurationSeconds =
			MinimumStableDurationSeconds;
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
	FOpenMobileSensorsActivityAmbiguityNormalizationTest,
	"OpenMobile.Sensors.Activity.Confidence.AmbiguityNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsActivityAmbiguityNormalizationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileActivitySensorSample Sample;
	Sample.Activity = static_cast<EOpenMobileMotionActivity>(200);
	Sample.Confidence = static_cast<EOpenMobileActivityConfidence>(200);
	Sample.ConcurrentActivities = {
		EOpenMobileMotionActivity::Walking,
		static_cast<EOpenMobileMotionActivity>(199),
		EOpenMobileMotionActivity::Running,
		EOpenMobileMotionActivity::Walking,
		EOpenMobileMotionActivity::Unknown
	};
	FOpenMobileMotionActivityClassifier::NormalizeSample(Sample);
	TestEqual(TEXT("The deterministic primary wins an ambiguous tie"),
		Sample.Activity, EOpenMobileMotionActivity::Running);
	TestEqual(TEXT("Unknown native confidence stays categorical"),
		Sample.Confidence, EOpenMobileActivityConfidence::Unknown);
	TestEqual(TEXT("Invalid and duplicate diagnostics are removed"),
		Sample.ConcurrentActivities.Num(), 2);
	if (Sample.ConcurrentActivities.Num() == 2)
	{
		TestEqual(TEXT("Walking remains in diagnostics"),
			Sample.ConcurrentActivities[0],
			EOpenMobileMotionActivity::Walking);
		TestEqual(TEXT("Running remains in diagnostics"),
			Sample.ConcurrentActivities[1],
			EOpenMobileMotionActivity::Running);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsActivityConfidenceFilterTest,
	"OpenMobile.Sensors.Activity.Confidence.Filter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsActivityConfidenceFilterTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsActivityConfidenceTestsPrivate;
	FOpenMobileActivitySampleFilter Filter;
	FOpenMobileActivityFilterConfig Config;
	Config.MinimumConfidence = EOpenMobileActivityConfidence::Medium;
	Config.MinimumStableDurationSeconds = 2.0;
	Filter.Configure(Config);
	FOpenMobileActivitySensorSample Sample = MakeSample(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::Low,
		1.0
	);
	TestFalse(TEXT("Low confidence is suppressed"), Filter.Process(Sample));
	Sample = MakeSample(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::High,
		2.0
	);
	TestFalse(TEXT("An unstable classification is suppressed"),
		Filter.Process(Sample));
	Sample.Header.TimestampSeconds = 3.0;
	TestTrue(TEXT("A stable classification is delivered"),
		Filter.Process(Sample));
	Sample = MakeSample(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::Medium,
		4.0
	);
	TestTrue(TEXT("Confidence changes emit without restarting stability"),
		Filter.Process(Sample));
	Sample.Header.TimestampSeconds = 5.0;
	TestFalse(TEXT("An identical eligible update is suppressed"),
		Filter.Process(Sample));
	Sample.Confidence = EOpenMobileActivityConfidence::Low;
	Sample.Header.TimestampSeconds = 6.0;
	TestFalse(TEXT("Confidence falling below the threshold is suppressed"),
		Filter.Process(Sample));
	Sample.Confidence = EOpenMobileActivityConfidence::Medium;
	Sample.Header.TimestampSeconds = 7.0;
	TestTrue(TEXT("Eligibility recovery emits the current state"),
		Filter.Process(Sample));
	Sample = MakeSample(
		EOpenMobileMotionActivity::Running,
		EOpenMobileActivityConfidence::High,
		8.0
	);
	TestFalse(TEXT("A new activity starts a stability window"),
		Filter.Process(Sample));
	Sample.Header.TimestampSeconds = 10.0;
	TestTrue(TEXT("The new activity emits after its stability window"),
		Filter.Process(Sample));
	Sample.Header.bStatefulProcessingReset = true;
	Sample.Header.TimestampSeconds = 11.0;
	TestFalse(TEXT("Lifecycle reset restarts the stability window"),
		Filter.Process(Sample));
	Sample.Header.bStatefulProcessingReset = false;
	Sample.Header.TimestampSeconds = 13.0;
	TestTrue(TEXT("The resumed state emits after becoming stable"),
		Filter.Process(Sample));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPerSubscriptionActivityConfidenceTest,
	"OpenMobile.Sensors.Activity.Confidence.PerSubscription",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPerSubscriptionActivityConfidenceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsActivityConfidenceTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ActivityConfidence"));
	Backend.SetSensorCapabilities({MakeCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid LowOwner = FGuid::NewGuid();
	const FGuid StableOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Low =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			LowOwner,
			MakeRequest(EOpenMobileActivityConfidence::Low, 0.0)
		);
	const FOpenMobileSensorSubscriptionResult Stable =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			StableOwner,
			MakeRequest(EOpenMobileActivityConfidence::High, 2.0)
		);
	TestEqual(TEXT("The low-confidence policy is applied"),
		Low.AppliedOptions.MinimumActivityConfidence,
		EOpenMobileActivityConfidence::Low);
	TestEqual(TEXT("The stability threshold is applied"),
		Stable.AppliedOptions.MinimumActivityStableDurationSeconds,
		2.0);
	FOpenMobileSensorSubscriptionRequest Invalid = MakeRequest(
		EOpenMobileActivityConfidence::Medium,
		std::numeric_limits<double>::quiet_NaN()
	);
	TestEqual(TEXT("A non-finite stability threshold is rejected"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			Invalid
		).Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	Invalid = MakeRequest(
		static_cast<EOpenMobileActivityConfidence>(200),
		0.0
	);
	TestEqual(TEXT("An unknown minimum confidence is rejected"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			Invalid
		).Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorReadResult ReadResult;
	FOpenMobileActivitySensorSample ReadSample;
	FOpenMobileSensorsSampleService::PublishActivity(MakeSample(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::Low,
		1.0
	));
	TestTrue(TEXT("The low-confidence subscriber receives the first state"),
		FOpenMobileSensorsSampleService::ReadLatestActivity(
			LowOwner,
			Low.Handle,
			0,
			1.0,
			ReadResult,
			ReadSample
		));
	TestFalse(TEXT("The strict subscriber suppresses low confidence"),
		FOpenMobileSensorsSampleService::ReadLatestActivity(
			StableOwner,
			Stable.Handle,
			0,
			1.0,
			ReadResult,
			ReadSample
		));
	FOpenMobileSensorsSampleService::PublishActivity(MakeSample(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::High,
		2.0
	));
	TestFalse(TEXT("High confidence alone does not bypass stability"),
		FOpenMobileSensorsSampleService::ReadLatestActivity(
			StableOwner,
			Stable.Handle,
			0,
			2.0,
			ReadResult,
			ReadSample
		));
	FOpenMobileSensorsSampleService::PublishActivity(MakeSample(
		EOpenMobileMotionActivity::Walking,
		EOpenMobileActivityConfidence::High,
		3.0
	));
	TestTrue(TEXT("The strict subscriber receives the stable state"),
		FOpenMobileSensorsSampleService::ReadLatestActivity(
			StableOwner,
			Stable.Handle,
			0,
			3.0,
			ReadResult,
			ReadSample
		));
	TestEqual(TEXT("Both subscribers share one physical stream"),
		Backend.GetStartSensorStreamCount(), 1);
	FinishBackend(Backend);
	return true;
}

#endif

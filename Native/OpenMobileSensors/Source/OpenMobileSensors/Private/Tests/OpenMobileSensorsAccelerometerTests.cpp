#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorValidity.h"
#include "OpenMobileSensorVectorFilter.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

#include <limits>

namespace OpenMobileSensorsAccelerometerTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(bool bFiltered)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 1.0;
		Request.Options.MaximumCallbackFrequencyHz = 1.0;
		Request.Options.Filters.bEnableLowPass = bFiltered;
		Request.Options.Filters.LowPassTimeConstantSeconds = 1.0;
		return Request;
	}

	FOpenMobileSensorSubscriptionResult StartActive(
		const FGuid& Owner,
		bool bFiltered
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				MakeRequest(bFiltered)
			);
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		return Result;
	}

	FOpenMobileVectorSensorSample MakeSample(
		double TimestampSeconds,
		double Value,
		EOpenMobileSensorSourceFlags Source =
			EOpenMobileSensorSourceFlags::CalibratedNative
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = MakeRequest(false).Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			Source
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
	FOpenMobileSensorsAccelerometerNativeRangeTest,
	"OpenMobile.Sensors.Accelerometer.NativeRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerNativeRangeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	TestTrue(TEXT("Values within the declared range are accepted"),
		FOpenMobileSensorValidity::IsWithinMaximumRange(
			FVector(4.0, -8.0, 16.0), 20.0));
	TestTrue(TEXT("A value exactly at the limit is accepted"),
		FOpenMobileSensorValidity::IsWithinMaximumRange(
			FVector(20.0, 0.0, 0.0), 20.0));
	TestFalse(TEXT("A component beyond the limit is rejected"),
		FOpenMobileSensorValidity::IsWithinMaximumRange(
			FVector(20.01, 0.0, 0.0), 20.0));
	TestFalse(TEXT("A nonfinite component is rejected"),
		FOpenMobileSensorValidity::IsWithinMaximumRange(
			FVector(
				std::numeric_limits<double>::quiet_NaN(),
				0.0,
				0.0),
			20.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccelerometerLowPassTest,
	"OpenMobile.Sensors.Accelerometer.LowPass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerLowPassTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorFilterOptions Options;
	Options.bEnableLowPass = true;
	Options.LowPassTimeConstantSeconds = 1.0;
	FOpenMobileSensorVectorFilter Filter;
	FOpenMobileVectorSensorSample Sample;
	Sample.Header.TimestampSeconds = 0.0;
	Sample.Header.bValid = true;
	Sample.Value = FVector::ZeroVector;
	TestTrue(TEXT("The first sample initializes the filter"),
		Filter.Apply(Options, Sample));
	Sample.Header.TimestampSeconds = 1.0;
	Sample.Value = FVector(10.0, 0.0, 0.0);
	TestTrue(TEXT("A later sample is filtered"),
		Filter.Apply(Options, Sample));
	TestEqual(TEXT("One-second input uses a one-half low-pass alpha"),
		Sample.Value, FVector(5.0, 0.0, 0.0));
	Sample.Header.TimestampSeconds = 1.5;
	Sample.Value = FVector(10.0, 0.0, 0.0);
	TestTrue(TEXT("A variable sample interval remains valid"),
		Filter.Apply(Options, Sample));
	TestTrue(TEXT("The variable interval uses elapsed time"),
		FMath::IsNearlyEqual(Sample.Value.X, 20.0 / 3.0, 1e-9));
	Sample.Header.TimestampSeconds = 2.0;
	Sample.Header.bStatefulProcessingReset = true;
	Sample.Value = FVector(10.0, 0.0, 0.0);
	TestTrue(TEXT("A reset sample reinitializes the filter"),
		Filter.Apply(Options, Sample));
	TestEqual(TEXT("Reset does not blend across a discontinuity"),
		Sample.Value, FVector(10.0, 0.0, 0.0));
	Sample.Header.TimestampSeconds = 2.5;
	Sample.Header.bValid = false;
	TestFalse(TEXT("Invalid input is rejected"),
		Filter.Apply(Options, Sample));
	Sample.Header.TimestampSeconds = 3.0;
	Sample.Header.bValid = true;
	Sample.Header.bStatefulProcessingReset = false;
	Sample.Value = FVector(4.0, 0.0, 0.0);
	TestTrue(TEXT("Valid input after invalid data is accepted"),
		Filter.Apply(Options, Sample));
	TestEqual(TEXT("Invalid input clears prior low-pass state"),
		Sample.Value, FVector(4.0, 0.0, 0.0));
	Options = {};
	Sample.Header.TimestampSeconds = 4.0;
	Sample.Header.bStatefulProcessingReset = false;
	Sample.Value = FVector(-7.0, 2.0, 4.0);
	TestTrue(TEXT("Disabled filtering accepts valid input"),
		Filter.Apply(Options, Sample));
	TestEqual(TEXT("Disabled filtering is an identity"),
		Sample.Value, FVector(-7.0, 2.0, 4.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccelerometerHighPassTest,
	"OpenMobile.Sensors.Accelerometer.HighPass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerHighPassTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorFilterOptions Options;
	Options.bEnableHighPass = true;
	Options.HighPassTimeConstantSeconds = 1.0;
	FOpenMobileSensorVectorFilter Filter;
	FOpenMobileVectorSensorSample Sample;
	Sample.Header.TimestampSeconds = 0.0;
	Sample.Header.bValid = true;
	Sample.Value = FVector(4.0, 0.0, 0.0);
	TestTrue(TEXT("The baseline sample initializes the high-pass filter"),
		Filter.Apply(Options, Sample));
	TestEqual(TEXT("A constant baseline starts at zero"),
		Sample.Value, FVector::ZeroVector);
	TestTrue(TEXT("The sample reports high-pass filtering"),
		Sample.bHighPassFiltered);
	TestTrue(TEXT("The initial state is warming up"),
		Sample.bHighPassFilterWarmingUp);
	Sample.Header.TimestampSeconds = 0.25;
	Sample.Value = FVector(4.0, 0.0, 0.0);
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Constant input remains rejected"),
		Sample.Value, FVector::ZeroVector);
	TestTrue(TEXT("Warm-up follows elapsed time"),
		Sample.bHighPassFilterWarmingUp);
	Sample.Header.TimestampSeconds = 1.0;
	Sample.Value = FVector(4.0, 0.0, 0.0);
	Filter.Apply(Options, Sample);
	TestFalse(TEXT("Warm-up ends after one time constant"),
		Sample.bHighPassFilterWarmingUp);
	Sample.Header.TimestampSeconds = 2.0;
	Sample.Value = FVector(14.0, 0.0, 0.0);
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("A one-second impulse uses one-half alpha"),
		Sample.Value, FVector(5.0, 0.0, 0.0));
	Sample.Header.TimestampSeconds = 2.5;
	Sample.Value = FVector(4.0, 0.0, 0.0);
	Filter.Apply(Options, Sample);
	TestTrue(TEXT("Variable intervals preserve the impulse response"),
		FMath::IsNearlyEqual(Sample.Value.X, -10.0 / 3.0, 1e-9));
	Sample.Header.TimestampSeconds = 3.0;
	Sample.Header.bValid = false;
	TestFalse(TEXT("Invalid high-pass input is rejected"),
		Filter.Apply(Options, Sample));
	Sample.Header.TimestampSeconds = 4.0;
	Sample.Header.bValid = true;
	Sample.Value = FVector(4.0, 0.0, 0.0);
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Input after a reset starts from zero"),
		Sample.Value, FVector::ZeroVector);
	TestTrue(TEXT("Reset restarts warm-up"),
		Sample.bHighPassFilterWarmingUp);
	Sample.Header.TimestampSeconds = 1000000000000.0;
	Sample.Value = FVector(1000000000000.0, 0.0, 0.0);
	TestTrue(TEXT("A very large valid interval is processed"),
		Filter.Apply(Options, Sample));
	TestTrue(TEXT("A very large interval remains finite"),
		FMath::IsFinite(Sample.Value.X));
	Filter.Reset();
	Sample.Header.TimestampSeconds = 0.0;
	Sample.Value = FVector(7.0, -2.0, 3.0);
	Options = {};
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Disabled high-pass filtering is an identity"),
		Sample.Value, FVector(7.0, -2.0, 3.0));
	TestFalse(TEXT("Disabled samples are not marked filtered"),
		Sample.bHighPassFiltered);
	TestFalse(TEXT("Disabled samples are not warming up"),
		Sample.bHighPassFilterWarmingUp);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccelerometerFilterChainTest,
	"OpenMobile.Sensors.Accelerometer.FilterChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerFilterChainTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorFilterOptions Options;
	Options.bEnableHighPass = true;
	Options.HighPassTimeConstantSeconds = 1.0;
	Options.DeadZone = 1.0;
	FOpenMobileSensorVectorFilter Filter;
	FOpenMobileVectorSensorSample Sample;
	Sample.Header.TimestampSeconds = 0.0;
	Sample.Header.bValid = true;
	Sample.Value = FVector::ZeroVector;
	Filter.Apply(Options, Sample);
	Sample.Header.TimestampSeconds = 1.0;
	Sample.Value = FVector(2.0, 0.0, 0.0);
	TestTrue(TEXT("A valid high-pass sample is processed"),
		Filter.Apply(Options, Sample));
	TestEqual(TEXT("Dead-zone equality suppresses the filtered boundary"),
		Sample.Value, FVector::ZeroVector);
	Sample.Header.TimestampSeconds = 2.0;
	Sample.Value = FVector(4.0, 0.0, 0.0);
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Values above the dead zone remain observable"),
		Sample.Value, FVector(1.5, 0.0, 0.0));

	Options = FOpenMobileSensorFilterOptions{};
	Options.bEnableExponentialSmoothing = true;
	Options.SmoothingTimeConstantSeconds = 1.0;
	Filter.Reset();
	Sample.Header.TimestampSeconds = 0.0;
	Sample.Value = FVector::ZeroVector;
	Filter.Apply(Options, Sample);
	Sample.Header.TimestampSeconds = 1.0;
	Sample.Value = FVector(10.0, 0.0, 0.0);
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Exponential smoothing uses elapsed time"),
		Sample.Value, FVector(5.0, 0.0, 0.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccelerometerPerSubscriptionFilterTest,
	"OpenMobile.Sensors.Accelerometer.PerSubscriptionFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerPerSubscriptionFilterTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAccelerometerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AccelerometerFilters"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid RawOwner = FGuid::NewGuid();
	const FGuid FilteredOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Raw =
		StartActive(RawOwner, false);
	const FOpenMobileSensorSubscriptionResult Filtered =
		StartActive(FilteredOwner, true);
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(1.0, 0.0));
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(2.0, 10.0));

	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample RawSample;
	FOpenMobileVectorSensorSample FilteredSample;
	TestTrue(TEXT("The unfiltered subscription has a sample"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			RawOwner,
			Raw.Handle,
			0,
			2.0,
			Read,
			RawSample));
	TestTrue(TEXT("The filtered subscription has a sample"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			FilteredOwner,
			Filtered.Handle,
			0,
			2.0,
			Read,
			FilteredSample));
	TestEqual(TEXT("The raw subscription keeps the normalized input"),
		RawSample.Value, FVector(10.0, 0.0, 0.0));
	TestEqual(TEXT("The filtered subscription has independent state"),
		FilteredSample.Value, FVector(5.0, 0.0, 0.0));
	TestEqual(TEXT("Filtering preserves source provenance"),
		FilteredSample.Header.SourceFlags,
		static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative));
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(20.0, 20.0));
	TestTrue(TEXT("The filtered subscription accepts after a long gap"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			FilteredOwner,
			Filtered.Handle,
			0,
			20.0,
			Read,
			FilteredSample));
	TestEqual(TEXT("A long gap starts a fresh low-pass state"),
		FilteredSample.Value, FVector(20.0, 0.0, 0.0));
	TestTrue(TEXT("The long-gap reset is observable"),
		FilteredSample.Header.bStatefulProcessingReset);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Filtered.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSampleService::SetSubscriptionState(
		Filtered.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(21.0, 30.0));
	TestTrue(TEXT("The resumed subscription has a sample"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			FilteredOwner,
			Filtered.Handle,
			0,
			21.0,
			Read,
			FilteredSample
		));
	TestEqual(TEXT("Lifecycle resume starts a fresh low-pass state"),
		FilteredSample.Value, FVector(30.0, 0.0, 0.0));
	FOpenMobileSensorsSampleService::PublishVector(MakeSample(
		22.0,
		40.0,
		EOpenMobileSensorSourceFlags::Raw
	));
	TestTrue(TEXT("The source-changed subscription has a sample"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			FilteredOwner,
			Filtered.Handle,
			0,
			22.0,
			Read,
			FilteredSample
		));
	TestEqual(TEXT("A source change starts a fresh low-pass state"),
		FilteredSample.Value, FVector(40.0, 0.0, 0.0));
	TestTrue(TEXT("The source reset is observable"),
		FilteredSample.Header.bStatefulProcessingReset);
	FinishBackend(Backend);
	return true;
}

#endif

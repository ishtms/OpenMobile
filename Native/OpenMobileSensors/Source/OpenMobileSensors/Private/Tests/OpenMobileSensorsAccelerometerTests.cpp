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
		double Value
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = MakeRequest(false).Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
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
	Sample.Header.TimestampSeconds = 2.0;
	Sample.Header.bStatefulProcessingReset = true;
	Sample.Value = FVector(10.0, 0.0, 0.0);
	TestTrue(TEXT("A reset sample reinitializes the filter"),
		Filter.Apply(Options, Sample));
	TestEqual(TEXT("Reset does not blend across a discontinuity"),
		Sample.Value, FVector(10.0, 0.0, 0.0));
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
	FinishBackend(Backend);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorUnits.h"

#include <limits>

namespace OpenMobileSensorsPedometerMetricTestsPrivate
{
	FOpenMobileStepsSensorSample MakeQuerySample()
	{
		FOpenMobileStepsSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::StepCounter;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = 10.0;
		Sample.Header.bValid = true;
		Sample.Count = 100;
		Sample.Origin = EOpenMobileStepCountOrigin::QueryInterval;
		Sample.OriginIdentifier = FGuid::NewGuid();
		Sample.bHasQueryInterval = true;
		Sample.QueryStartUnixTimeSeconds = 1000.0;
		Sample.QueryEndUnixTimeSeconds = 1010.0;
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPedometerPartialMetricsTest,
	"OpenMobile.Sensors.Steps.PedometerMetrics.Partial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPedometerPartialMetricsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPedometerMetricTestsPrivate;
	FOpenMobileStepsSensorSample Sample = MakeQuerySample();
	Sample.Metrics.bHasDistanceMeters = true;
	Sample.Metrics.DistanceMeters = 125.5;
	Sample.Metrics.bHasFloorsAscended = true;
	Sample.Metrics.FloorsAscended = 5000000000LL;
	Sample.Metrics.FloorsDescended = 4;
	Sample.Metrics.PaceSecondsPerMeter = 3.5;
	Sample.Metrics.bHasCadenceStepsPerSecond = true;
	Sample.Metrics.CadenceStepsPerSecond = 2.25;
	TestTrue(TEXT("A partial native metric set normalizes"),
		FOpenMobileSensorUnitConverter::NormalizeStepsSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample));
	TestTrue(TEXT("Distance remains available"),
		Sample.Metrics.bHasDistanceMeters);
	TestEqual(TEXT("Distance remains in metres"),
		Sample.Metrics.DistanceMeters, 125.5);
	TestEqual(TEXT("Wide floor counts are preserved"),
		Sample.Metrics.FloorsAscended, 5000000000LL);
	TestFalse(TEXT("A missing descended count stays absent"),
		Sample.Metrics.bHasFloorsDescended);
	TestEqual(TEXT("Missing descended storage is canonical"),
		Sample.Metrics.FloorsDescended, 0LL);
	TestFalse(TEXT("A missing pace stays absent"),
		Sample.Metrics.bHasPaceSecondsPerMeter);
	TestEqual(TEXT("Missing pace storage is canonical"),
		Sample.Metrics.PaceSecondsPerMeter, 0.0);
	TestTrue(TEXT("Cadence remains independently available"),
		Sample.Metrics.bHasCadenceStepsPerSecond);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPedometerInvalidMetricsTest,
	"OpenMobile.Sensors.Steps.PedometerMetrics.Invalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPedometerInvalidMetricsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPedometerMetricTestsPrivate;
	FOpenMobileStepsSensorSample NegativeFloors = MakeQuerySample();
	NegativeFloors.Metrics.bHasFloorsAscended = true;
	NegativeFloors.Metrics.FloorsAscended = -1;
	TestFalse(TEXT("A negative native floor count is rejected"),
		FOpenMobileSensorUnitConverter::NormalizeStepsSample(
			EOpenMobileSensorNativePlatform::IOS,
			NegativeFloors));
	TestFalse(TEXT("Rejected floor data invalidates the sample"),
		NegativeFloors.Header.bValid);

	FOpenMobileStepsSensorSample InvalidDistance = MakeQuerySample();
	InvalidDistance.Metrics.bHasDistanceMeters = true;
	InvalidDistance.Metrics.DistanceMeters =
		std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("A nonfinite native distance is rejected"),
		FOpenMobileSensorUnitConverter::NormalizeStepsSample(
			EOpenMobileSensorNativePlatform::IOS,
			InvalidDistance));
	return true;
}

#endif

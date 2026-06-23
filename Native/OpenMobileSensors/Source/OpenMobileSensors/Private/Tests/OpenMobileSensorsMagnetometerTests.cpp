#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorValidity.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

#include <limits>

namespace OpenMobileSensorsMagnetometerTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Magnetometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		return Request;
	}

	FOpenMobileSensorCapability MakeCapability(EOpenMobileSensorType Type)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 100.0;
		return Capability;
	}

	FOpenMobileVectorSensorSample MakeSample(
		double TimestampSeconds,
		const FVector& MagneticField
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = MakeRequest().Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
		);
		Sample.Value = MagneticField;
		return Sample;
	}

	FOpenMobileVectorSensorBatch MakeBatch(
		double TimestampSeconds,
		const FVector& MagneticField
	)
	{
		FOpenMobileVectorSensorBatch Batch;
		Batch.Samples.Add(MakeSample(TimestampSeconds, MagneticField));
		return Batch;
	}

	FOpenMobileSensorSubscriptionResult StartActive(const FGuid& Owner)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				MakeRequest()
			);
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		return Result;
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
	FOpenMobileSensorsMagnetometerAxisMagnitudeTest,
	"OpenMobile.Sensors.Magnetometer.AxisMagnitude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMagnetometerAxisMagnitudeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		FOpenMobileVectorSensorSample Field;
		Field.Header.Sensor =
			OpenMobileSensorsMagnetometerTestsPrivate::MakeRequest().Sensor;
		Field.Header.bValid = true;
		Field.Value = FVector(30.0, 40.0, 0.0);
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(Platform, Field);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			Platform,
			Field
		);
		TestEqual(TEXT("Magnetic field uses the axial device transform"),
			Field.Value, FVector(-40.0, -30.0, 0.0));
		TestTrue(TEXT("Magnetic magnitude remains 50 microteslas"),
			FMath::IsNearlyEqual(Field.Value.Size(), 50.0, 1.e-9));
		TestTrue(TEXT("Magnetic field reports normalized units"),
			Field.Header.bUnitsNormalized);
	}
	TestTrue(TEXT("Magnetic field is not a heading stream"),
		FOpenMobileSensorTypes::GetStableName(
			EOpenMobileSensorType::Magnetometer) !=
		FOpenMobileSensorTypes::GetStableName(
			EOpenMobileSensorType::MagneticHeading));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMagnetometerInvalidAndSaturationTest,
	"OpenMobile.Sensors.Magnetometer.InvalidAndSaturation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMagnetometerInvalidAndSaturationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMagnetometerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("MagneticValidity"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(Owner);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeSample(1.0, FVector(25.0, 10.0, 40.0))
	);
	FOpenMobileVectorSensorSample Saturated =
		MakeSample(2.0, FVector(100.01, 0.0, 0.0));
	Saturated.Header.bValid &= FOpenMobileSensorValidity::IsWithinMaximumRange(
		Saturated.Value,
		100.0
	);
	FOpenMobileSensorsSampleService::PublishVector(Saturated);
	FOpenMobileVectorSensorSample Nonfinite = MakeSample(
		3.0,
		FVector(
			0.0,
			std::numeric_limits<double>::quiet_NaN(),
			0.0
		)
	);
	FOpenMobileSensorUnitConverter::NormalizeVectorSample(
		EOpenMobileSensorNativePlatform::Android,
		Nonfinite
	);
	FOpenMobileSensorsSampleService::PublishVector(Nonfinite);
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Latest;
	TestTrue(TEXT("The last valid magnetic field remains readable"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			0,
			3.0,
			Read,
			Latest
		));
	TestEqual(TEXT("Invalid magnetic samples do not replace valid data"),
		Latest.Value, FVector(25.0, 10.0, 40.0));
	TestEqual(TEXT("Rejected magnetic samples do not advance sequence"),
		Latest.Header.Sequence, 1ll);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMagnetometerInterferenceQualityTest,
	"OpenMobile.Sensors.Magnetometer.InterferenceQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMagnetometerInterferenceQualityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMagnetometerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("MagneticInterference"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(Owner);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	const FOpenMobileSensorIdentifier Sensor = MakeRequest().Sensor;
	TestTrue(TEXT("Interference quality is accepted before its sample"),
		FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
			Token,
			PhysicalHandle,
			FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
				Sensor,
				-1,
				1.0
			)
		));
	TestTrue(TEXT("The affected magnetic sample is accepted"),
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			Token,
			PhysicalHandle,
			MakeBatch(1.0, FVector(80.0, -20.0, 35.0))
		));
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Latest;
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner,
		Subscription.Handle,
		0,
		1.1,
		Read,
		Latest
	);
	TestEqual(TEXT("Interference remains visible on the affected sample"),
		Latest.Header.Accuracy, EOpenMobileSensorAccuracy::Unreliable);
	TestTrue(TEXT("Interference requests calibration"),
		Latest.Header.bCalibrationRequired);
	TestEqual(TEXT("Quality does not smooth away the measured field"),
		Latest.Value, FVector(80.0, -20.0, 35.0));

	FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
		Token,
		PhysicalHandle,
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor,
			2,
			2.0
		)
	);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		PhysicalHandle,
		MakeBatch(2.0, FVector(30.0, 5.0, 45.0))
	);
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner,
		Subscription.Handle,
		Latest.Header.Sequence,
		2.1,
		Read,
		Latest
	);
	TestEqual(TEXT("Recovered magnetic quality reaches its next sample"),
		Latest.Header.Accuracy, EOpenMobileSensorAccuracy::High);
	TestFalse(TEXT("Recovered magnetic quality clears calibration guidance"),
		Latest.Header.bCalibrationRequired);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMagnetometerUnavailableTest,
	"OpenMobile.Sensors.Magnetometer.Unavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMagnetometerUnavailableTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMagnetometerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("NoMagnetometer"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Magnetometer =
		Snapshot.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::Magnetometer;
			}
		);
	TestNotNull(TEXT("The matrix retains a missing magnetometer"),
		Magnetometer);
	if (Magnetometer)
	{
		TestEqual(TEXT("Missing magnetometer hardware is unavailable"),
			Magnetometer->Availability.State,
			EOpenMobileCapabilityState::Unavailable);
		TestEqual(TEXT("Missing magnetometer has a typed restriction"),
			Magnetometer->ActiveRestriction,
			EOpenMobileSensorRestriction::MissingHardware);
	}
	FinishBackend(Backend);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorLinearAccelerationEstimator.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsLinearAccelerationTestsPrivate
{
	constexpr double Gravity = 9.80665;

	FOpenMobileVectorSensorSample MakeAcceleration(
		double TimestampSeconds,
		const FVector& Value,
		EOpenMobileSensorAccuracy Accuracy =
			EOpenMobileSensorAccuracy::High,
		int32 SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
		)
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.bValid = true;
		Sample.Header.Accuracy = Accuracy;
		Sample.Header.SourceFlags = SourceFlags;
		Sample.Value = Value;
		return Sample;
	}

	FOpenMobileSensorIdentifier MakeLinearSensor()
	{
		FOpenMobileSensorIdentifier Sensor;
		Sensor.Type = EOpenMobileSensorType::LinearAcceleration;
		Sensor.InstanceId = TEXT("Default");
		return Sensor;
	}

	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 15.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		bool bAllowFallback = true
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor = MakeLinearSensor();
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.bAllowDerivedFallback = bAllowFallback;
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
	FOpenMobileSensorsLinearAccelerationEstimatorMotionTest,
	"OpenMobile.Sensors.LinearAcceleration.EstimatorMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLinearAccelerationEstimatorMotionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLinearAccelerationTestsPrivate;
	FOpenMobileSensorLinearAccelerationEstimator Estimator;
	FOpenMobileVectorSensorSample Output;
	TestTrue(TEXT("A stationary sample produces fallback output"),
		Estimator.Process(
			MakeAcceleration(1.0, FVector(0.0, 0.0, -Gravity)),
			MakeLinearSensor(),
			Output
		));
	TestEqual(TEXT("Stillness removes gravity"),
		Output.Value, FVector::ZeroVector);
	TestTrue(TEXT("The first output marks a filter reset"),
		Output.Header.bStatefulProcessingReset);
	TestTrue(TEXT("Fallback output reports filter lag"),
		Output.Header.Fusion.bHasEstimatedLag);
	TestEqual(TEXT("Fallback lag matches the bounded filter"),
		Output.Header.Fusion.EstimatedLagSeconds, 0.5);
	TestEqual(TEXT("Fallback quality is not equivalent to native fusion"),
		Output.Header.Fusion.Quality,
		EOpenMobileSensorFusionQuality::Degraded);

	Estimator.Process(
		MakeAcceleration(1.02, FVector(5.0, 0.0, -Gravity)),
		MakeLinearSensor(),
		Output
	);
	TestTrue(TEXT("A brief impulse remains visible"),
		Output.Value.X > 4.0);
	for (int32 Index = 2; Index <= 300; ++Index)
	{
		Estimator.Process(
			MakeAcceleration(
				1.0 + Index / 60.0,
				FVector(5.0, 0.0, -Gravity)
			),
			MakeLinearSensor(),
			Output
		);
	}
	TestTrue(TEXT("Sustained acceleration is absorbed by the fallback"),
		FMath::Abs(Output.Value.X) < 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLinearAccelerationEstimatorValidityTest,
	"OpenMobile.Sensors.LinearAcceleration.EstimatorValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLinearAccelerationEstimatorValidityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLinearAccelerationTestsPrivate;
	FOpenMobileSensorLinearAccelerationEstimator Estimator;
	FOpenMobileVectorSensorSample Output;
	Estimator.Process(
		MakeAcceleration(1.0, FVector(0.0, 0.0, -Gravity)),
		MakeLinearSensor(),
		Output
	);
	TestTrue(TEXT("Insufficient gravity quality still produces metadata"),
		Estimator.Process(
			MakeAcceleration(
				1.02,
				FVector(0.0, 0.0, -Gravity),
				EOpenMobileSensorAccuracy::Low
			),
			MakeLinearSensor(),
			Output
		));
	TestFalse(TEXT("Insufficient gravity quality marks output invalid"),
		Output.Header.bValid);
	TestTrue(TEXT("Invalid quality reports the accelerometer as degraded"),
		UOpenMobileSensorQualityLibrary::ContainsSensor(
			Output.Header.Fusion.DegradedInputMask,
			EOpenMobileSensorType::Accelerometer
		));
	TestTrue(TEXT("Valid input after invalid quality restarts the filter"),
		Estimator.Process(
			MakeAcceleration(1.04, FVector(0.0, -Gravity, 0.0)),
			MakeLinearSensor(),
			Output
		));
	TestEqual(TEXT("Restart does not mix the old gravity pose"),
		Output.Value, FVector::ZeroVector);
	TestTrue(TEXT("Restart is marked"),
		Output.Header.bStatefulProcessingReset);
	Estimator.Process(
		MakeAcceleration(2.0, FVector(-Gravity, 0.0, 0.0)),
		MakeLinearSensor(),
		Output
	);
	TestEqual(TEXT("A long gap starts from the new pose"),
		Output.Value, FVector::ZeroVector);
	TestEqual(TEXT("Output stays aligned to the input timestamp"),
		Output.Header.TimestampSeconds, 2.0);
	TestTrue(TEXT("A long gap marks a filter reset"),
		Output.Header.bStatefulProcessingReset);
	TestTrue(TEXT("Raw acceleration cannot claim calibrated fallback"),
		Estimator.Process(
			MakeAcceleration(
				2.02,
				FVector(-Gravity, 0.0, 0.0),
				EOpenMobileSensorAccuracy::High,
				static_cast<int32>(EOpenMobileSensorSourceFlags::Raw)
			),
			MakeLinearSensor(),
			Output
		));
	TestFalse(TEXT("Raw acceleration produces invalid fallback output"),
		Output.Header.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLinearAccelerationSelectionTest,
	"OpenMobile.Sensors.LinearAcceleration.NativeAndFallbackSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLinearAccelerationSelectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLinearAccelerationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend NativeBackend(TEXT("NativeLinear"));
	NativeBackend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::LinearAcceleration)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(NativeBackend);
	const FGuid NativeOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Native =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			NativeOwner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Native linear acceleration is preferred"),
		NativeBackend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::LinearAcceleration);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		NativeOwner,
		Native.Handle
	);
	FinishBackend(NativeBackend);

	FOpenMobileSensorsMockBackend FallbackBackend(TEXT("DerivedLinear"));
	FallbackBackend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(FallbackBackend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Fallback =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Missing native linear acceleration uses accelerometer"),
		FallbackBackend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::Accelerometer);
	const FOpenMobileSensorCapabilitySnapshot Capabilities =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* LinearCapability =
		Capabilities.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::LinearAcceleration;
			}
		);
	TestNotNull(TEXT("Fallback publishes a linear acceleration capability"),
		LinearCapability);
	if (LinearCapability)
	{
		TestEqual(TEXT("Fallback capability reports derived provenance"),
			LinearCapability->Source,
			EOpenMobileSensorAvailabilitySource::Derived);
	}
	FOpenMobileVectorSensorBatch Batch;
	Batch.Samples.Add(MakeAcceleration(
		1.0,
		FVector(0.0, 0.0, -Gravity)
	));
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		FallbackBackend.GetLastStartedPhysicalHandle(),
		Batch
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Derived;
	TestTrue(TEXT("Fallback publishes a linear acceleration sample"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Fallback.Handle,
			0,
			1.1,
			Read,
			Derived
		));
	TestEqual(TEXT("Fallback retains the logical sensor"),
		Derived.Header.Sensor.Type,
		EOpenMobileSensorType::LinearAcceleration);
	TestEqual(TEXT("Fallback retains the aligned timestamp"),
		Derived.Header.TimestampSeconds, 1.0);
	TestTrue(TEXT("Fallback reports its lag"),
		Derived.Header.Fusion.bHasEstimatedLag);
	FinishBackend(FallbackBackend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsLinearAccelerationUnsupportedTest,
	"OpenMobile.Sensors.LinearAcceleration.UnsupportedFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsLinearAccelerationUnsupportedTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsLinearAccelerationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("NoLinearInput"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorSubscriptionResult Missing =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("Fallback without accelerometer is unavailable"),
		Missing.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsCapabilityService::HandleBackendGenerationChanged();
	const FOpenMobileSensorSubscriptionResult Disabled =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest(false)
		);
	TestEqual(TEXT("Disabled fallback does not weaken the request"),
		Disabled.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	FinishBackend(Backend);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorGravityEstimator.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsGravityTestsPrivate
{
	constexpr double Gravity = 9.80665;

	FOpenMobileVectorSensorSample MakeAcceleration(
		double TimestampSeconds,
		const FVector& Value,
		EOpenMobileSensorAccuracy Accuracy =
			EOpenMobileSensorAccuracy::Unknown,
		bool bValid = true
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.bValid = bValid;
		Sample.Header.Accuracy = Accuracy;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
		);
		Sample.Value = Value;
		return Sample;
	}

	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		EOpenMobileSensorAvailabilitySource Source =
			EOpenMobileSensorAvailabilitySource::Native
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = Source;
		Capability.MinimumFrequencyHz = 15.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeGravityRequest(
		bool bAllowFallback = true
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Gravity;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.bAllowDerivedFallback = bAllowFallback;
		return Request;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsScreenRotationService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsScreenRotationService::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGravityEstimatorStationaryTiltTest,
	"OpenMobile.Sensors.Gravity.EstimatorStationaryTilt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGravityEstimatorStationaryTiltTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGravityTestsPrivate;
	FOpenMobileSensorGravityEstimator Estimator;
	FOpenMobileVectorSensorSample Output;
	FOpenMobileSensorIdentifier GravitySensor;
	GravitySensor.Type = EOpenMobileSensorType::Gravity;
	GravitySensor.InstanceId = TEXT("Default");
	TestTrue(TEXT("A stationary pose initializes the fallback"),
		Estimator.Process(
			MakeAcceleration(1.0, FVector(-Gravity, 0.0, 0.0)),
			GravitySensor,
			Output
		));
	TestEqual(TEXT("The stationary gravity pose is retained"),
		Output.Value, FVector(-Gravity, 0.0, 0.0));
	TestTrue(TEXT("The first derived sample resets stateful consumers"),
		Output.Header.bStatefulProcessingReset);
	TestEqual(TEXT("Gravity fallback is plugin-derived"),
		Output.Header.SourceFlags,
		static_cast<int32>(EOpenMobileSensorSourceFlags::PluginDerived));
	TestEqual(TEXT("Accelerometer is the contributing input"),
		Output.Header.Fusion.ContributingInputMask,
		UOpenMobileSensorQualityLibrary::MakeInputMask(
			EOpenMobileSensorType::Accelerometer));

	for (int32 Index = 1; Index <= 120; ++Index)
	{
		const double Alpha = static_cast<double>(Index) / 120.0;
		const double Angle = Alpha * UE_DOUBLE_PI * 0.5;
		Estimator.Process(
			MakeAcceleration(
				1.0 + Index / 60.0,
				FVector(
					-Gravity * FMath::Cos(Angle),
					-Gravity * FMath::Sin(Angle),
					0.0
				)
			),
			GravitySensor,
			Output
		);
	}
	TestTrue(TEXT("The fallback follows a slow tilt"),
		Output.Value.Y < -8.0 && FMath::Abs(Output.Value.X) < 4.0);
	TestTrue(TEXT("Slow tilt retains nominal observable quality"),
		Output.Header.Fusion.Quality ==
			EOpenMobileSensorFusionQuality::Nominal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGravityEstimatorContaminationResetTest,
	"OpenMobile.Sensors.Gravity.EstimatorContaminationReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGravityEstimatorContaminationResetTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGravityTestsPrivate;
	FOpenMobileSensorGravityEstimator Estimator;
	FOpenMobileVectorSensorSample Output;
	FOpenMobileSensorIdentifier GravitySensor;
	GravitySensor.Type = EOpenMobileSensorType::Gravity;
	GravitySensor.InstanceId = TEXT("Default");
	Estimator.Process(
		MakeAcceleration(1.0, FVector(0.0, 0.0, -Gravity)),
		GravitySensor,
		Output
	);
	Estimator.Process(
		MakeAcceleration(1.02, FVector(20.0, 0.0, -Gravity)),
		GravitySensor,
		Output
	);
	TestTrue(TEXT("A brief linear impulse is bounded by the low pass"),
		Output.Value.X < 1.0);
	TestEqual(TEXT("Linear contamination degrades fusion quality"),
		Output.Header.Fusion.Quality,
		EOpenMobileSensorFusionQuality::Degraded);
	TestFalse(TEXT("Unreliable input is not used for fallback"),
		Estimator.Process(
			MakeAcceleration(
				1.04,
				FVector(0.0, 0.0, -Gravity),
				EOpenMobileSensorAccuracy::Unreliable
			),
			GravitySensor,
			Output
		));
	TestTrue(TEXT("A valid sample after invalid quality restarts fallback"),
		Estimator.Process(
			MakeAcceleration(1.06, FVector(0.0, -Gravity, 0.0)),
			GravitySensor,
			Output
		));
	TestEqual(TEXT("Restart does not blend across invalid input"),
		Output.Value, FVector(0.0, -Gravity, 0.0));
	TestTrue(TEXT("Restart is visible to stateful consumers"),
		Output.Header.bStatefulProcessingReset);
	Estimator.Process(
		MakeAcceleration(2.0, FVector(-Gravity, 0.0, 0.0)),
		GravitySensor,
		Output
	);
	TestEqual(TEXT("A long gap starts from the new pose"),
		Output.Value, FVector(-Gravity, 0.0, 0.0));
	TestTrue(TEXT("A long gap carries a reset marker"),
		Output.Header.bStatefulProcessingReset);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGravityNativeAndFallbackSelectionTest,
	"OpenMobile.Sensors.Gravity.NativeAndFallbackSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGravityNativeAndFallbackSelectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGravityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend NativeBackend(TEXT("NativeGravity"));
	NativeBackend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::Gravity)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(NativeBackend);
	const FGuid NativeOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Native =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			NativeOwner,
			MakeGravityRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("A native gravity sensor is preferred"),
		NativeBackend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::Gravity);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		NativeOwner,
		Native.Handle
	);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(NativeBackend);
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsSampleService::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();

	FOpenMobileSensorsMockBackend SlowNativeBackend(TEXT("SlowNativeGravity"));
	FOpenMobileSensorCapability SlowNativeGravity =
		MakeCapability(EOpenMobileSensorType::Gravity);
	SlowNativeGravity.MaximumFrequencyHz = 10.0;
	SlowNativeBackend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		SlowNativeGravity
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(SlowNativeBackend);
	const FOpenMobileSensorCapabilitySnapshot SlowNativeCapabilities =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* SlowGravityCapability =
		SlowNativeCapabilities.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::Gravity;
			}
		);
	TestNotNull(TEXT("The slow native source has a gravity capability"),
		SlowGravityCapability);
	if (SlowGravityCapability)
	{
		TestEqual(TEXT("An insufficient native rate selects derived gravity"),
			SlowGravityCapability->Source,
			EOpenMobileSensorAvailabilitySource::Derived);
	}
	const FOpenMobileSensorSubscriptionResult SlowNative =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeGravityRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("A slow native gravity source uses the fallback"),
		SlowNativeBackend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::Accelerometer);
	FinishBackend(SlowNativeBackend);

	FOpenMobileSensorsMockBackend FallbackBackend(TEXT("DerivedGravity"));
	FallbackBackend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(FallbackBackend);
	const FGuid FallbackOwner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		FallbackOwner,
		EOpenMobileSensorScreenRotation::Rotation90,
		0.5,
		false
	);
	FOpenMobileSensorSubscriptionRequest FallbackRequest =
		MakeGravityRequest();
	FallbackRequest.Options.CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::CurrentScreen;
	const FOpenMobileSensorSubscriptionResult Fallback =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FallbackOwner,
			FallbackRequest
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Missing native gravity falls back to accelerometer"),
		FallbackBackend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::Accelerometer);
	const FOpenMobileSensorCapabilitySnapshot FallbackCapabilities =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* GravityCapability =
		FallbackCapabilities.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::Gravity;
			}
		);
	TestNotNull(TEXT("The fallback publishes a gravity capability"),
		GravityCapability);
	if (GravityCapability)
	{
		TestEqual(TEXT("The fallback capability identifies derived output"),
			GravityCapability->Source,
			EOpenMobileSensorAvailabilitySource::Derived);
	}
	FOpenMobileVectorSensorBatch Batch;
	Batch.Samples.Add(MakeAcceleration(
		1.0,
		FVector(Gravity, 0.0, 0.0),
		EOpenMobileSensorAccuracy::Medium
	));
	TestTrue(TEXT("The fallback accepts its accelerometer batch"),
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			FOpenMobileSensorsBackendRegistry::CaptureToken(),
			FallbackBackend.GetLastStartedPhysicalHandle(),
			Batch
		));
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Derived;
	TestTrue(TEXT("The fallback produces a gravity sample"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			FallbackOwner,
			Fallback.Handle,
			0,
			1.1,
			Read,
			Derived
		));
	TestEqual(TEXT("The fallback retains its logical sensor"),
		Derived.Header.Sensor.Type, EOpenMobileSensorType::Gravity);
	TestEqual(TEXT("The fallback retains the input timestamp"),
		Derived.Header.TimestampSeconds, 1.0);
	TestEqual(TEXT("The fallback applies requested screen coordinates"),
		Derived.Header.CoordinateSpace,
		EOpenMobileSensorCoordinateSpace::CurrentScreen);
	TestEqual(TEXT("The fallback records the applied screen rotation"),
		Derived.Header.ScreenRotation,
		EOpenMobileSensorScreenRotation::Rotation90);
	TestEqual(TEXT("The fallback rotates its derived value at delivery"),
		Derived.Value, FVector(0.0, -Gravity, 0.0));
	TestEqual(TEXT("The fallback retains input accuracy"),
		Derived.Header.Accuracy,
		EOpenMobileSensorAccuracy::Medium);
	TestEqual(TEXT("The fallback reports its derived source"),
		Derived.Header.SourceFlags,
		static_cast<int32>(EOpenMobileSensorSourceFlags::PluginDerived));
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Fallback.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Fallback.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	const FVector RestartValue(0.0, Gravity, 0.0);
	FOpenMobileVectorSensorBatch RestartBatch;
	RestartBatch.Samples.Add(MakeAcceleration(
		1.1,
		RestartValue,
		EOpenMobileSensorAccuracy::Medium
	));
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		FallbackBackend.GetLastStartedPhysicalHandle(),
		RestartBatch
	);
	FOpenMobileVectorSensorSample Restarted;
	FOpenMobileSensorsSampleService::ReadLatestVector(
		FallbackOwner,
		Fallback.Handle,
		Derived.Header.Sequence,
		1.2,
		Read,
		Restarted
	);
	TestEqual(TEXT("A resumed fallback starts from the new pose"),
		Restarted.Value,
		FOpenMobileSensorsScreenRotationService::RotateVector(
			RestartValue,
			EOpenMobileSensorScreenRotation::Rotation90
		));
	TestTrue(TEXT("A resumed fallback marks its filter reset"),
		Restarted.Header.bStatefulProcessingReset);
	FinishBackend(FallbackBackend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGravityUnsupportedFallbackTest,
	"OpenMobile.Sensors.Gravity.UnsupportedFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGravityUnsupportedFallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGravityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("NoGravityInputs"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorSubscriptionResult MissingInputs =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeGravityRequest()
		);
	TestEqual(TEXT("Fallback without accelerometer input is unavailable"),
		MissingInputs.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	FOpenMobileSensorCapability SlowAccelerometer =
		MakeCapability(EOpenMobileSensorType::Accelerometer);
	SlowAccelerometer.MaximumFrequencyHz = 10.0;
	Backend.SetSensorCapabilities({SlowAccelerometer});
	FOpenMobileSensorsCapabilityService::HandleBackendGenerationChanged();
	const FOpenMobileSensorSubscriptionResult InsufficientRate =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeGravityRequest()
		);
	TestEqual(TEXT("A slow accelerometer cannot drive the fallback"),
		InsufficientRate.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsCapabilityService::HandleBackendGenerationChanged();
	const FOpenMobileSensorSubscriptionResult DisabledFallback =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeGravityRequest(false)
		);
	TestEqual(TEXT("A disabled fallback does not weaken the request"),
		DisabledFallback.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDerivedFallbackPhysicalSharingTest,
	"OpenMobile.Sensors.DerivedFallback.PhysicalSharing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedFallbackPhysicalSharingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGravityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DerivedFallbackSharing"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest AccelerometerRequest;
	AccelerometerRequest.Sensor.Type = EOpenMobileSensorType::Accelerometer;
	AccelerometerRequest.Sensor.InstanceId = TEXT("Default");
	AccelerometerRequest.Options.RatePreset =
		EOpenMobileSensorRatePreset::Custom;
	AccelerometerRequest.Options.CustomFrequencyHz = 30.0;
	AccelerometerRequest.Options.bAllowDerivedFallback = false;
	FOpenMobileSensorSubscriptionRequest GravityRequest =
		MakeGravityRequest();
	GravityRequest.Options.CustomFrequencyHz = 60.0;
	FOpenMobileSensorSubscriptionRequest LinearRequest = GravityRequest;
	LinearRequest.Sensor.Type = EOpenMobileSensorType::LinearAcceleration;
	LinearRequest.Options.CustomFrequencyHz = 90.0;
	const FOpenMobileSensorSubscriptionResult Accelerometer =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			AccelerometerRequest
		);
	const FOpenMobileSensorSubscriptionResult GravitySubscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			GravityRequest
		);
	const FOpenMobileSensorSubscriptionResult LinearSubscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			LinearRequest
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Direct and derived consumers share one stream"),
		Backend.GetStartSensorStreamCount(), 1);
	TestEqual(TEXT("One physical stream serves all consumers"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		1);
	TestEqual(TEXT("The shared stream uses the fastest requested rate"),
		Backend.GetLastStartedPhysicalRequest().RequestedFrequencyHz,
		90.0);
	TestTrue(TEXT("Stopping one fallback succeeds"),
		FOpenMobileSensorsSubscriptionService::StopSubscription(
			Owner,
			GravitySubscription.Handle
		).IsSuccess());
	TestEqual(TEXT("A remaining consumer keeps the stream active"),
		Backend.GetStopSensorStreamCount(), 0);
	TestTrue(TEXT("Stopping the direct consumer succeeds"),
		FOpenMobileSensorsSubscriptionService::StopSubscription(
			Owner,
			Accelerometer.Handle
		).IsSuccess());
	TestEqual(TEXT("The final fallback still owns the stream"),
		Backend.GetStopSensorStreamCount(), 0);
	TestTrue(TEXT("Stopping the final fallback succeeds"),
		FOpenMobileSensorsSubscriptionService::StopSubscription(
			Owner,
			LinearSubscription.Handle
		).IsSuccess());
	TestEqual(TEXT("The final consumer stops the physical stream"),
		Backend.GetStopSensorStreamCount(), 1);
	TestEqual(TEXT("No hidden stream remains"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDerivedFallbackDescriptorTest,
	"OpenMobile.Sensors.DerivedFallback.Descriptors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedFallbackDescriptorTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGravityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DerivedFallbackDescriptors"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::Gravity),
		MakeCapability(EOpenMobileSensorType::LinearAcceleration)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* GravityCapability =
		Snapshot.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::Gravity;
			}
		);
	const FOpenMobileSensorCapability* LinearCapability =
		Snapshot.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::LinearAcceleration;
			}
		);
	TestNotNull(TEXT("Gravity capability is present"), GravityCapability);
	TestNotNull(TEXT("Linear acceleration capability is present"),
		LinearCapability);
	if (GravityCapability && LinearCapability)
	{
		TestEqual(TEXT("Native gravity remains preferred"),
			GravityCapability->Source,
			EOpenMobileSensorAvailabilitySource::Native);
		TestTrue(TEXT("Gravity declares an implemented fallback"),
			GravityCapability->Fallback.bImplemented);
		TestTrue(TEXT("Gravity fallback is available on this device"),
			GravityCapability->Fallback.bAvailable);
		TestEqual(TEXT("Gravity declares one fallback input"),
			GravityCapability->Fallback.RequiredInputs.Num(), 1);
		if (GravityCapability->Fallback.RequiredInputs.Num() == 1)
		{
			TestEqual(TEXT("Gravity requires acceleration"),
				GravityCapability->Fallback.RequiredInputs[0],
				EOpenMobileSensorType::Accelerometer);
		}
		TestEqual(TEXT("Gravity declares the minimum input rate"),
			GravityCapability->Fallback.MinimumInputFrequencyHz, 15.0);
		TestFalse(TEXT("Gravity does not require calibrated acceleration"),
			GravityCapability->Fallback.bRequiresCalibratedInput);
		TestEqual(TEXT("Gravity declares nominal expected quality"),
			GravityCapability->Fallback.ExpectedQuality,
			EOpenMobileSensorFusionQuality::Nominal);
		TestEqual(TEXT("Gravity declares bounded power cost"),
			GravityCapability->Fallback.PowerCost,
			EOpenMobileSensorFallbackPowerCost::Low);
		TestTrue(TEXT("Gravity declares a finite CPU budget"),
			FMath::IsFinite(
				GravityCapability->Fallback.CpuBudgetMicrosecondsPerSample
			)
			&& GravityCapability->Fallback.
				CpuBudgetMicrosecondsPerSample > 0.0
			&& GravityCapability->Fallback.
				CpuBudgetMicrosecondsPerSample <= 100.0);
		const int32 MissingInput = static_cast<int32>(
			EOpenMobileSensorFallbackUnsupportedCondition::MissingInput
		);
		const int32 InsufficientRate = static_cast<int32>(
			EOpenMobileSensorFallbackUnsupportedCondition::InsufficientRate
		);
		TestTrue(TEXT("Gravity declares missing-input rejection"),
			(GravityCapability->Fallback.UnsupportedConditionFlags
				& MissingInput) != 0);
		TestTrue(TEXT("Gravity declares insufficient-rate rejection"),
			(GravityCapability->Fallback.UnsupportedConditionFlags
				& InsufficientRate) != 0);
		TestTrue(TEXT("Linear fallback is available on this device"),
			LinearCapability->Fallback.bAvailable);
		TestTrue(TEXT("Linear fallback requires calibrated input"),
			LinearCapability->Fallback.bRequiresCalibratedInput);
		TestEqual(TEXT("Linear fallback declares degraded quality"),
			LinearCapability->Fallback.ExpectedQuality,
			EOpenMobileSensorFusionQuality::Degraded);
		TestTrue(TEXT("Linear fallback declares calibration rejection"),
			(LinearCapability->Fallback.UnsupportedConditionFlags
				& static_cast<int32>(
					EOpenMobileSensorFallbackUnsupportedCondition::
						UncalibratedInput
				)) != 0);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDerivedFallbackSourceChangeTest,
	"OpenMobile.Sensors.DerivedFallback.SourceChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedFallbackSourceChangeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGravityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DerivedFallbackSourceChange"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeGravityRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileVectorSensorBatch Batch;
	Batch.Samples.Add(MakeAcceleration(
		1.0,
		FVector(0.0, 0.0, -Gravity),
		EOpenMobileSensorAccuracy::High
	));
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		Batch
	);
	FOpenMobileVectorSensorSample RawInput = MakeAcceleration(
		2.0,
		FVector(0.0, 0.0, -Gravity),
		EOpenMobileSensorAccuracy::High
	);
	RawInput.Header.SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::Raw
	);
	Batch.Samples.Reset();
	Batch.Samples.Add(RawInput);
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		Batch
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Output;
	TestTrue(TEXT("The changed input still produces gravity"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			0,
			2.1,
			Read,
			Output
		));
	TestEqual(TEXT("The public source remains plugin-derived"),
		Output.Header.SourceFlags,
		static_cast<int32>(EOpenMobileSensorSourceFlags::PluginDerived));
	TestTrue(TEXT("The hidden input-source change is broadcast"),
		Output.Header.bSourceChanged);
	TestTrue(TEXT("The input-source change resets the estimator"),
		Output.Header.bStatefulProcessingReset);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDerivedFallbackAvailabilityRecoveryTest,
	"OpenMobile.Sensors.DerivedFallback.AvailabilityRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedFallbackAvailabilityRecoveryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGravityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DerivedFallbackRecovery"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Initial =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	int32 ChangeCount = 0;
	const FDelegateHandle ChangedHandle =
		FOpenMobileSensorsCapabilityService::OnChanged().AddLambda(
			[&ChangeCount](
				const FOpenMobileSensorCapabilitySnapshot& Snapshot
			)
			{
				static_cast<void>(Snapshot);
				++ChangeCount;
			}
		);
	Backend.SetSensorCapabilities({});
	FOpenMobileSensorsCapabilityService::HandleBackendGenerationChanged();
	const FOpenMobileSensorCapabilitySnapshot Lost =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* LostGravity =
		Lost.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::Gravity;
			}
		);
	TestNotNull(TEXT("Gravity remains discoverable after input loss"),
		LostGravity);
	if (LostGravity)
	{
		TestTrue(TEXT("The fallback contract remains declared"),
			LostGravity->Fallback.bImplemented);
		TestFalse(TEXT("The lost input makes fallback unavailable"),
			LostGravity->Fallback.bAvailable);
		TestEqual(TEXT("Gravity is unavailable without its input"),
			LostGravity->Availability.State,
			EOpenMobileCapabilityState::Unavailable);
	}
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsCapabilityService::HandleBackendGenerationChanged();
	const FOpenMobileSensorCapabilitySnapshot Recovered =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* RecoveredGravity =
		Recovered.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::Gravity;
			}
		);
	TestNotNull(TEXT("Gravity is rediscovered after recovery"),
		RecoveredGravity);
	if (RecoveredGravity)
	{
		TestTrue(TEXT("The recovered fallback becomes available"),
			RecoveredGravity->Fallback.bAvailable);
		TestEqual(TEXT("Recovery selects the derived source"),
			RecoveredGravity->Source,
			EOpenMobileSensorAvailabilitySource::Derived);
		TestEqual(TEXT("Recovery restores availability"),
			RecoveredGravity->Availability.State,
			EOpenMobileCapabilityState::Available);
	}
	TestEqual(TEXT("Input loss and recovery are both broadcast"),
		ChangeCount, 2);
	TestTrue(TEXT("Recovery restores the original capability matrix"),
		Initial == Recovered);
	FOpenMobileSensorsCapabilityService::OnChanged().Remove(ChangedHandle);
	FinishBackend(Backend);
	return true;
}

#endif

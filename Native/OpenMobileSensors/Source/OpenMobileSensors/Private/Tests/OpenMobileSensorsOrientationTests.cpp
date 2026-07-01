#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorOrientationClassifier.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsOrientationTestsPrivate
{
	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State =
			EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 15.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}

	FOpenMobileVectorSensorSample MakeAcceleration(
		double TimestampSeconds,
		const FVector& Value
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.Accuracy = EOpenMobileSensorAccuracy::High;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
		);
		Sample.Value = Value;
		return Sample;
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
	FOpenMobileSensorsOrientationPoseClassifierTest,
	"OpenMobile.Sensors.Orientation.Classifier.Poses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsOrientationPoseClassifierTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorOrientationClassifier Classifier;
	FOpenMobileSensorOrientationClassifierConfig Config;
	Config.FaceAngleDegrees = 25.0;
	Config.HysteresisDegrees = 5.0;
	Config.TransitionDebounceSeconds = 0.0;
	struct FCase
	{
		FVector Gravity;
		EOpenMobilePhysicalOrientation Expected;
	};
	const FCase Cases[] = {
		{FVector(-1.0, 0.0, 0.0),
			EOpenMobilePhysicalOrientation::Portrait},
		{FVector(1.0, 0.0, 0.0),
			EOpenMobilePhysicalOrientation::PortraitUpsideDown},
		{FVector(0.0, -1.0, 0.0),
			EOpenMobilePhysicalOrientation::LandscapeLeft},
		{FVector(0.0, 1.0, 0.0),
			EOpenMobilePhysicalOrientation::LandscapeRight},
		{FVector(0.0, 0.0, -1.0),
			EOpenMobilePhysicalOrientation::FaceUp},
		{FVector(0.0, 0.0, 1.0),
			EOpenMobilePhysicalOrientation::FaceDown}
	};
	double TimestampSeconds = 1.0;
	for (const FCase& TestCase : Cases)
	{
		Classifier.Reset();
		FOpenMobileOrientationSensorSample Output;
		TestTrue(TEXT("A canonical pose is classified"),
			Classifier.Process(
				TestCase.Gravity,
				TimestampSeconds,
				Config,
				Output
			));
		TestEqual(TEXT("The canonical pose has the expected orientation"),
			Output.Orientation,
			TestCase.Expected);
		TestEqual(TEXT("A canonical pose has full confidence"),
			Output.Confidence, 1.0);
		TimestampSeconds += 1.0;
	}
	Classifier.Reset();
	FOpenMobileOrientationSensorSample Ambiguous;
	TestTrue(TEXT("A diagonal pose produces an explicit state"),
		Classifier.Process(
			FVector(1.0, 1.0, 0.0),
			TimestampSeconds,
			Config,
			Ambiguous
		));
	TestEqual(TEXT("A diagonal edge pose is unknown"),
		Ambiguous.Orientation,
		EOpenMobilePhysicalOrientation::Unknown);
	TestTrue(TEXT("Ambiguous confidence remains normalized"),
		Ambiguous.Confidence >= 0.0 && Ambiguous.Confidence <= 1.0);
	Classifier.Reset();
	FOpenMobileOrientationSensorSample FlatBoundary;
	TestTrue(TEXT("A flat boundary produces an explicit state"),
		Classifier.Process(
			FVector(1.0, 0.0, 1.0),
			TimestampSeconds + 1.0,
			Config,
			FlatBoundary
		));
	TestEqual(TEXT("A flat-boundary pose is unknown"),
		FlatBoundary.Orientation,
		EOpenMobilePhysicalOrientation::Unknown);
	Classifier.Reset();
	FOpenMobileOrientationSensorSample Invalid;
	TestTrue(TEXT("Missing gravity produces an explicit state"),
		Classifier.Process(
			FVector::ZeroVector,
			TimestampSeconds + 2.0,
			Config,
			Invalid
		));
	TestEqual(TEXT("Missing gravity is unknown"),
		Invalid.Orientation,
		EOpenMobilePhysicalOrientation::Unknown);
	TestEqual(TEXT("Missing gravity has zero confidence"),
		Invalid.Confidence, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsOrientationDebounceTest,
	"OpenMobile.Sensors.Orientation.Classifier.DebounceAndBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsOrientationDebounceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorOrientationClassifier Classifier;
	FOpenMobileSensorOrientationClassifierConfig Config;
	Config.FaceAngleDegrees = 25.0;
	Config.HysteresisDegrees = 5.0;
	Config.TransitionDebounceSeconds = 0.2;
	FOpenMobileOrientationSensorSample Output;
	Classifier.Process(
		-FVector::ForwardVector,
		1.0,
		Config,
		Output
	);
	TestEqual(TEXT("A new pose waits for debounce"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Unknown);
	TestEqual(TEXT("An uncommitted pose has zero confidence"),
		Output.Confidence, 0.0);
	Classifier.Process(
		-FVector::ForwardVector,
		1.1,
		Config,
		Output
	);
	TestEqual(TEXT("An early stable pose remains pending"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Unknown);
	Classifier.Process(
		-FVector::ForwardVector,
		1.21,
		Config,
		Output
	);
	TestEqual(TEXT("A stable pose commits after debounce"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Portrait);
	const double BoundaryA = FMath::DegreesToRadians(42.0);
	Classifier.Process(
		FVector(-FMath::Cos(BoundaryA), -FMath::Sin(BoundaryA), 0.0),
		1.25,
		Config,
		Output
	);
	TestEqual(TEXT("Boundary noise does not change the current pose"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Portrait);
	const double BoundaryB = FMath::DegreesToRadians(48.0);
	Classifier.Process(
		FVector(-FMath::Cos(BoundaryB), -FMath::Sin(BoundaryB), 0.0),
		1.35,
		Config,
		Output
	);
	TestEqual(TEXT("Noise across the diagonal does not chatter"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Portrait);
	Classifier.Process(
		-FVector::RightVector,
		1.4,
		Config,
		Output
	);
	TestEqual(TEXT("Rapid movement starts a pending transition"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Portrait);
	Classifier.Process(
		-FVector::ForwardVector,
		1.5,
		Config,
		Output
	);
	TestEqual(TEXT("Returning before debounce cancels the transition"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Portrait);
	Classifier.Process(
		-FVector::RightVector,
		1.6,
		Config,
		Output
	);
	Classifier.Process(
		-FVector::RightVector,
		1.81,
		Config,
		Output
	);
	TestEqual(TEXT("A stable landscape pose eventually commits"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::LandscapeLeft);
	TestEqual(TEXT("A canonical committed pose has full confidence"),
		Output.Confidence, 1.0);
	Classifier.Process(
		-FVector::UpVector,
		1.9,
		Config,
		Output
	);
	Classifier.Process(
		-FVector::UpVector,
		2.11,
		Config,
		Output
	);
	TestEqual(TEXT("A stable flat pose commits independently"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::FaceUp);
	Classifier.Process(
		-FVector::ForwardVector,
		2.0,
		Config,
		Output
	);
	TestEqual(TEXT("A backward timestamp resets classification"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Unknown);
	TestTrue(TEXT("A backward timestamp marks a state reset"),
		Output.Header.bStatefulProcessingReset);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDerivedOrientationDeliveryTest,
	"OpenMobile.Sensors.Orientation.DerivedDelivery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedOrientationDeliveryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsOrientationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DerivedOrientation"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner,
		EOpenMobileSensorScreenRotation::Rotation90,
		0.5,
		false
	);
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::PhysicalOrientation;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	Request.Options.CustomFrequencyHz = 30.0;
	Request.Options.CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::CurrentScreen;
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	TestTrue(TEXT("Derived orientation subscription is accepted"),
		Subscription.Operation.IsSuccess());
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Orientation resolves to the accelerometer"),
		Backend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::Accelerometer);
	FOpenMobileVectorSensorBatch Batch;
	for (const double TimestampSeconds : {1.0, 1.1, 1.21})
	{
		Batch.Samples.Add(MakeAcceleration(
			TimestampSeconds,
			FVector(-9.80665, 0.0, 0.0)
		));
	}
	TestTrue(TEXT("The physical batch is accepted"),
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			FOpenMobileSensorsBackendRegistry::CaptureToken(),
			Backend.GetLastStartedPhysicalHandle(),
			Batch
		));
	FOpenMobileSensorReadResult Read;
	FOpenMobileOrientationSensorSample Output;
	TestTrue(TEXT("The derived orientation can be read"),
		FOpenMobileSensorsSampleService::ReadLatestOrientation(
			Owner,
			Subscription.Handle,
			0,
			1.3,
			Read,
			Output
		));
	TestEqual(TEXT("Natural upright pose is portrait"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Portrait);
	TestEqual(TEXT("Orientation retains its logical sensor"),
		Output.Header.Sensor.Type,
		EOpenMobileSensorType::PhysicalOrientation);
	TestEqual(TEXT("Orientation stays device-fixed"),
		Output.Header.CoordinateSpace,
		EOpenMobileSensorCoordinateSpace::DeviceFixed);
	TestEqual(TEXT("Window rotation is not applied"),
		Output.Header.ScreenRotation,
		EOpenMobileSensorScreenRotation::Rotation0);
	TestEqual(TEXT("Derived orientation reports plugin provenance"),
		Output.Header.SourceFlags,
		static_cast<int32>(EOpenMobileSensorSourceFlags::PluginDerived));
	TestEqual(TEXT("A canonical pose has full confidence"),
		Output.Confidence, 1.0);
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner,
		EOpenMobileSensorScreenRotation::Rotation180,
		1.25,
		false
	);
	FOpenMobileVectorSensorBatch RotatedWindowBatch;
	RotatedWindowBatch.Samples.Add(MakeAcceleration(
		1.3,
		FVector(-9.80665, 0.0, 0.0)
	));
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		RotatedWindowBatch
	);
	FOpenMobileSensorsSampleService::ReadLatestOrientation(
		Owner,
		Subscription.Handle,
		Output.Header.Sequence,
		1.4,
		Read,
		Output
	);
	TestEqual(TEXT("An orientation-locked window cannot change the pose"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Portrait);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsOrientationSourceSelectionTest,
	"OpenMobile.Sensors.Orientation.SourceSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsOrientationSourceSelectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsOrientationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend NativeBackend(TEXT("NativeOrientation"));
	NativeBackend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::PhysicalOrientation)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(NativeBackend);
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::PhysicalOrientation;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	Request.Options.CustomFrequencyHz = 30.0;
	const FGuid NativeOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Native =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			NativeOwner,
			Request
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("A native physical orientation source is preferred"),
		NativeBackend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::PhysicalOrientation);
	FOpenMobileOrientationSensorBatch NativeBatch;
	FOpenMobileOrientationSensorSample NativeSample;
	NativeSample.Header.Sensor = Request.Sensor;
	NativeSample.Header.TimestampSeconds = 1.0;
	NativeSample.Header.bValid = true;
	NativeSample.Header.bUnitsNormalized = true;
	NativeSample.Header.bCoordinatesNormalized = true;
	NativeSample.Header.SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::Raw
	);
	NativeSample.Orientation =
		EOpenMobilePhysicalOrientation::LandscapeRight;
	NativeSample.Confidence = 0.8;
	NativeBatch.Samples.Add(NativeSample);
	FOpenMobileSensorsSampleService::PublishOrientationBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		NativeBackend.GetLastStartedPhysicalHandle(),
		NativeBatch
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileOrientationSensorSample Output;
	TestTrue(TEXT("A native orientation sample is delivered"),
		FOpenMobileSensorsSampleService::ReadLatestOrientation(
			NativeOwner,
			Native.Handle,
			0,
			1.1,
			Read,
			Output
		));
	TestEqual(TEXT("Native orientation is mapped exactly"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::LandscapeRight);
	TestEqual(TEXT("Native confidence is retained"),
		Output.Confidence, 0.8);
	FinishBackend(NativeBackend);

	FOpenMobileSensorsMockBackend FallbackBackend(TEXT("OrientationFallback"));
	FallbackBackend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(FallbackBackend);
	const FOpenMobileSensorCapabilitySnapshot Capabilities =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* OrientationCapability =
		Capabilities.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::PhysicalOrientation;
			}
		);
	TestNotNull(TEXT("Derived orientation is discoverable"),
		OrientationCapability);
	if (OrientationCapability)
	{
		TestEqual(TEXT("The capability reports derived provenance"),
			OrientationCapability->Source,
			EOpenMobileSensorAvailabilitySource::Derived);
		TestTrue(TEXT("The fallback is reported available"),
			OrientationCapability->Fallback.bAvailable);
		TestEqual(TEXT("The fallback declares one input"),
			OrientationCapability->Fallback.RequiredInputs.Num(), 1);
		if (OrientationCapability->Fallback.RequiredInputs.Num() == 1)
		{
			TestEqual(TEXT("The fallback declares acceleration input"),
				OrientationCapability->Fallback.RequiredInputs[0],
				EOpenMobileSensorType::Accelerometer);
		}
	}
	Request.Options.bAllowDerivedFallback = false;
	const FOpenMobileSensorSubscriptionResult Disabled =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			Request
		);
	TestEqual(TEXT("Disabled fallback does not start hidden input"),
		Disabled.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	TestEqual(TEXT("Disabled fallback starts no physical stream"),
		FallbackBackend.GetStartSensorStreamCount(), 0);
	FinishBackend(FallbackBackend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsOrientationLifecycleResetTest,
	"OpenMobile.Sensors.Orientation.LifecycleReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsOrientationLifecycleResetTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsOrientationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("OrientationLifecycle"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::PhysicalOrientation;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	Request.Options.CustomFrequencyHz = 30.0;
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileVectorSensorBatch PortraitBatch;
	PortraitBatch.Samples.Add(MakeAcceleration(
		1.0,
		FVector(-9.80665, 0.0, 0.0)
	));
	PortraitBatch.Samples.Add(MakeAcceleration(
		1.21,
		FVector(-9.80665, 0.0, 0.0)
	));
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		PortraitBatch
	);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileVectorSensorBatch ResumedBatch;
	ResumedBatch.Samples.Add(MakeAcceleration(
		2.0,
		FVector(0.0, -9.80665, 0.0)
	));
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		ResumedBatch
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileOrientationSensorSample Output;
	FOpenMobileSensorsSampleService::ReadLatestOrientation(
		Owner,
		Subscription.Handle,
		0,
		2.1,
		Read,
		Output
	);
	TestEqual(TEXT("Resume does not reuse the previous orientation"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::Unknown);
	TestTrue(TEXT("The first resumed sample marks a reset"),
		Output.Header.bStatefulProcessingReset);
	ResumedBatch.Samples.Reset();
	ResumedBatch.Samples.Add(MakeAcceleration(
		2.21,
		FVector(0.0, -9.80665, 0.0)
	));
	FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		ResumedBatch
	);
	FOpenMobileSensorsSampleService::ReadLatestOrientation(
		Owner,
		Subscription.Handle,
		Output.Header.Sequence,
		2.3,
		Read,
		Output
	);
	TestEqual(TEXT("A stable resumed pose commits after debounce"),
		Output.Orientation,
		EOpenMobilePhysicalOrientation::LandscapeLeft);
	FinishBackend(Backend);
	return true;
}

#endif

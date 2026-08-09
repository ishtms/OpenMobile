#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenMobileSensorShakeDetector.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsShakeTestsPrivate
{
	FOpenMobileShakeDetectionOptions MakeConfig(int32 MinimumImpulses = 3)
	{
		FOpenMobileShakeDetectionOptions Config;
		Config.StrengthThresholdMetresPerSecondSquared = 5.0;
		Config.MinimumImpulses = MinimumImpulses;
		Config.DurationWindowSeconds = 1.0;
		Config.QuietResetSeconds = 0.05;
		Config.CooldownSeconds = 1.0;
		return Config;
	}

	FOpenMobileVectorSensorSample MakeLinearAcceleration(
		double TimestampSeconds,
		const FVector& Value
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor.Type =
			EOpenMobileSensorType::LinearAcceleration;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
		);
		Sample.Value = Value;
		return Sample;
	}

	FOpenMobileVectorSensorSample MakeAcceleration(
		double TimestampSeconds,
		const FVector& Value
	)
	{
		FOpenMobileVectorSensorSample Sample = MakeLinearAcceleration(
			TimestampSeconds,
			Value
		);
		Sample.Header.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.Accuracy = EOpenMobileSensorAccuracy::High;
		return Sample;
	}

	FOpenMobileSensorCapability MakeCapability(EOpenMobileSensorType Type)
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

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorType Type
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = Type;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.MaximumCallbackFrequencyHz = 60.0;
		Request.Options.DeliveryMode =
			EOpenMobileSensorDeliveryMode::Buffered;
		Request.Options.ShakeDetection = MakeConfig();
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
	FOpenMobileSensorsShakeStateMachineTest,
	"OpenMobile.Sensors.Shake.StateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShakeStateMachineTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsShakeTestsPrivate;
	FOpenMobileSensorShakeDetector Detector;
	Detector.Configure(MakeConfig());
	FOpenMobileVectorSensorSample Event;
	TestFalse(TEXT("Quiet input does not emit"),
		Detector.Process(MakeLinearAcceleration(
			0.0,
			FVector::ZeroVector
		), Event));
	TestFalse(TEXT("One bump does not emit"),
		Detector.Process(MakeLinearAcceleration(
			0.1,
			FVector(6.0, 0.0, 0.0)
		), Event));
	TestFalse(TEXT("Sustained acceleration is one impulse"),
		Detector.Process(MakeLinearAcceleration(
			0.15,
			FVector(7.0, 0.0, 0.0)
		), Event));
	Detector.Process(MakeLinearAcceleration(0.2, FVector::ZeroVector), Event);
	Detector.Process(MakeLinearAcceleration(0.26, FVector::ZeroVector), Event);
	TestFalse(TEXT("Two impulses remain below the configured minimum"),
		Detector.Process(MakeLinearAcceleration(
			0.3,
			FVector(0.0, -6.0, 0.0)
		), Event));
	Detector.Process(MakeLinearAcceleration(0.4, FVector::ZeroVector), Event);
	Detector.Process(MakeLinearAcceleration(0.46, FVector::ZeroVector), Event);
	TestTrue(TEXT("Three bounded impulses emit one shake"),
		Detector.Process(MakeLinearAcceleration(
			0.5,
			FVector(0.0, 0.0, 8.0)
		), Event));
	TestTrue(TEXT("The vector sample contains a shake event"),
		Event.bHasShakeEvent);
	TestEqual(TEXT("The event reports all qualifying impulses"),
		Event.ShakeEvent.ImpulseCount, 3);
	TestEqual(TEXT("The event reports peak strength"),
		Event.ShakeEvent.StrengthMetresPerSecondSquared, 8.0);
	TestTrue(TEXT("The event duration follows sample timestamps"),
		FMath::IsNearlyEqual(Event.ShakeEvent.DurationSeconds, 0.4, 1.e-9));
	TestEqual(TEXT("The event includes its detection timestamp"),
		Event.ShakeEvent.TimestampSeconds, 0.5);
	TestEqual(TEXT("The event identifies its physical source"),
		Event.ShakeEvent.SourceSensor.Type,
		EOpenMobileSensorType::LinearAcceleration);
	TestEqual(TEXT("Shake output uses the logical sensor identity"),
		Event.Header.Sensor.Type, EOpenMobileSensorType::Shake);
	TestTrue(TEXT("Shake provenance is plugin derived"),
		(Event.Header.SourceFlags & static_cast<int32>(
			EOpenMobileSensorSourceFlags::PluginDerived
		)) != 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShakeConfigurationValidationTest,
	"OpenMobile.Sensors.Shake.ConfigurationValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShakeConfigurationValidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsShakeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ShakeValidation"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::LinearAcceleration)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	auto TestRejected = [this, &Owner](
		const TCHAR* What,
		const FOpenMobileShakeDetectionOptions& Options
	)
	{
		FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
			EOpenMobileSensorType::Shake
		);
		Request.Options.ShakeDetection = Options;
		TestEqual(
			What,
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				Request
			).Operation.Code,
			EOpenMobileSensorResultCode::InvalidArgument
		);
	};
	FOpenMobileShakeDetectionOptions Invalid = MakeConfig();
	Invalid.StrengthThresholdMetresPerSecondSquared =
		std::numeric_limits<double>::quiet_NaN();
	TestRejected(TEXT("A non-finite threshold is rejected"), Invalid);
	Invalid = MakeConfig();
	Invalid.MinimumImpulses = 0;
	TestRejected(TEXT("An empty impulse requirement is rejected"), Invalid);
	Invalid = MakeConfig();
	Invalid.DurationWindowSeconds = 0.0;
	TestRejected(TEXT("A zero duration window is rejected"), Invalid);
	Invalid = MakeConfig();
	Invalid.QuietResetSeconds = -1.0;
	TestRejected(TEXT("A negative quiet reset is rejected"), Invalid);
	Invalid = MakeConfig();
	Invalid.CooldownSeconds = -1.0;
	TestRejected(TEXT("A negative cooldown is rejected"), Invalid);
	TestEqual(TEXT("Validation does not start a physical stream"),
		Backend.GetStartSensorStreamCount(), 0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShakeCooldownLifecycleTest,
	"OpenMobile.Sensors.Shake.CooldownAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShakeCooldownLifecycleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsShakeTestsPrivate;
	FOpenMobileSensorShakeDetector Detector;
	Detector.Configure(MakeConfig(1));
	FOpenMobileVectorSensorSample Event;
	TestTrue(TEXT("The first qualifying impulse emits"),
		Detector.Process(MakeLinearAcceleration(
			1.0,
			FVector(6.0, 0.0, 0.0)
		), Event));
	Detector.Process(MakeLinearAcceleration(1.1, FVector::ZeroVector), Event);
	Detector.Process(MakeLinearAcceleration(1.16, FVector::ZeroVector), Event);
	TestFalse(TEXT("Cooldown prevents event spam"),
		Detector.Process(MakeLinearAcceleration(
			1.5,
			FVector(0.0, 6.0, 0.0)
		), Event));
	Detector.Process(MakeLinearAcceleration(1.6, FVector::ZeroVector), Event);
	Detector.Process(MakeLinearAcceleration(1.66, FVector::ZeroVector), Event);
	TestTrue(TEXT("The cooldown boundary is inclusive"),
		Detector.Process(MakeLinearAcceleration(
			2.0,
			FVector(0.0, 0.0, 6.0)
		), Event));
	FOpenMobileVectorSensorSample Reset = MakeLinearAcceleration(
		2.1,
		FVector::ZeroVector
	);
	Reset.Header.bStatefulProcessingReset = true;
	Detector.Process(Reset, Event);
	TestTrue(TEXT("Lifecycle reset clears the old cooldown"),
		Detector.Process(MakeLinearAcceleration(
			2.2,
			FVector(-6.0, 0.0, 0.0)
		), Event));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShakeOrientationTest,
	"OpenMobile.Sensors.Shake.OrientationIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShakeOrientationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsShakeTestsPrivate;
	const FVector Fixtures[] = {
		FVector(6.0, 0.0, 0.0),
		FVector(0.0, -6.0, 0.0),
		FVector(0.0, 0.0, 6.0),
		FVector(-3.464101615, 3.464101615, 3.464101615)
	};
	for (const FVector& Fixture : Fixtures)
	{
		FOpenMobileSensorShakeDetector Detector;
		Detector.Configure(MakeConfig(1));
		FOpenMobileVectorSensorSample Event;
		TestTrue(TEXT("A rotated fixture emits"),
			Detector.Process(MakeLinearAcceleration(
				1.0,
				Fixture
			), Event));
		TestTrue(TEXT("Rotated strength uses vector magnitude"),
			FMath::IsNearlyEqual(
				Event.ShakeEvent.StrengthMetresPerSecondSquared,
				6.0,
				1.e-6
			));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShakeSharedStreamTest,
	"OpenMobile.Sensors.Shake.SharedStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShakeSharedStreamTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsShakeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ShakeSharedStream"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::LinearAcceleration)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Capabilities =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* ShakeCapability =
		Capabilities.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type == EOpenMobileSensorType::Shake;
			}
		);
	TestNotNull(TEXT("Shake has a derived capability"), ShakeCapability);
	if (ShakeCapability)
	{
		TestEqual(TEXT("Shake is available through fallback"),
			ShakeCapability->Availability.State,
			EOpenMobileCapabilityState::Available);
		TestEqual(TEXT("Shake is explicitly derived"),
			ShakeCapability->Source,
			EOpenMobileSensorAvailabilitySource::Derived);
	}
	const FGuid LinearOwner = FGuid::NewGuid();
	const FGuid ShakeOwner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Linear =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			LinearOwner,
			MakeRequest(EOpenMobileSensorType::LinearAcceleration)
		);
	const FOpenMobileSensorSubscriptionResult Shake =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			ShakeOwner,
			MakeRequest(EOpenMobileSensorType::Shake)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("The shake subscription is accepted"),
		Shake.Operation.Code, EOpenMobileSensorResultCode::Accepted);
	TestEqual(TEXT("Shake prefers native linear acceleration"),
		Backend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::LinearAcceleration);
	TestEqual(TEXT("Direct and shake consumers share one stream"),
		Backend.GetStartSensorStreamCount(), 1);
	TestEqual(TEXT("Only one physical stream is retained"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		1);
	FOpenMobileVectorSensorBatch Batch;
	Batch.Samples = {
		MakeLinearAcceleration(1.0, FVector::ZeroVector),
		MakeLinearAcceleration(1.1, FVector(6.0, 0.0, 0.0)),
		MakeLinearAcceleration(1.2, FVector::ZeroVector),
		MakeLinearAcceleration(1.26, FVector::ZeroVector),
		MakeLinearAcceleration(1.3, FVector(0.0, 6.0, 0.0)),
		MakeLinearAcceleration(1.4, FVector::ZeroVector),
		MakeLinearAcceleration(1.46, FVector::ZeroVector),
		MakeLinearAcceleration(1.5, FVector(0.0, 0.0, 8.0))
	};
	TestTrue(TEXT("The shared physical batch is accepted"),
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			FOpenMobileSensorsBackendRegistry::CaptureToken(),
			Backend.GetLastStartedPhysicalHandle(),
			Batch
		));
	FOpenMobileVectorSensorBatch ShakeBatch;
	FOpenMobileSensorBufferReadResult Read;
	TestTrue(TEXT("The shake event is buffered"),
		FOpenMobileSensorsSampleService::DrainBufferedVector(
			ShakeOwner,
			Shake.Handle,
			8,
			Read,
			ShakeBatch
		));
	TestEqual(TEXT("Only the detected shake is delivered"),
		ShakeBatch.Samples.Num(), 1);
	if (ShakeBatch.Samples.Num() == 1)
	{
		TestEqual(TEXT("The delivered sample is a shake"),
			ShakeBatch.Samples[0].Header.Sensor.Type,
			EOpenMobileSensorType::Shake);
		TestEqual(TEXT("The shared stream preserves the impulse count"),
			ShakeBatch.Samples[0].ShakeEvent.ImpulseCount,
			3);
	}
	FOpenMobileVectorSensorSample LatestLinear;
	FOpenMobileSensorReadResult LatestRead;
	TestTrue(TEXT("The direct consumer keeps the latest input"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			LinearOwner,
			Linear.Handle,
			0,
			1.5,
			LatestRead,
			LatestLinear
		));
	TestEqual(TEXT("Direct delivery remains unmodified"),
		LatestLinear.Value, FVector(0.0, 0.0, 8.0));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShakeAccelerationFallbackTest,
	"OpenMobile.Sensors.Shake.AccelerationFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShakeAccelerationFallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsShakeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ShakeAccelerationFallback"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Capabilities =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* ShakeCapability =
		Capabilities.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type == EOpenMobileSensorType::Shake;
			}
		);
	TestNotNull(TEXT("Acceleration provides shake capability"), ShakeCapability);
	if (ShakeCapability)
	{
		TestEqual(TEXT("One fallback input is selected"),
			ShakeCapability->Fallback.RequiredInputs.Num(), 1);
		if (ShakeCapability->Fallback.RequiredInputs.Num() == 1)
		{
			TestEqual(TEXT("Acceleration is the selected fallback input"),
				ShakeCapability->Fallback.RequiredInputs[0],
				EOpenMobileSensorType::Accelerometer);
		}
	}
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Shake =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(EOpenMobileSensorType::Shake)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("The acceleration-backed request is accepted"),
		Shake.Operation.Code, EOpenMobileSensorResultCode::Accepted);
	TestEqual(TEXT("The physical stream uses acceleration"),
		Backend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::Accelerometer);
	constexpr double Gravity = 9.80665;
	FOpenMobileVectorSensorBatch Batch;
	Batch.Samples = {
		MakeAcceleration(1.0, FVector(0.0, 0.0, Gravity)),
		MakeAcceleration(1.1, FVector(8.0, 0.0, Gravity)),
		MakeAcceleration(1.2, FVector(0.0, 0.0, Gravity)),
		MakeAcceleration(1.26, FVector(0.0, 0.0, Gravity)),
		MakeAcceleration(1.3, FVector(0.0, 8.0, Gravity)),
		MakeAcceleration(1.4, FVector(0.0, 0.0, Gravity)),
		MakeAcceleration(1.46, FVector(0.0, 0.0, Gravity)),
		MakeAcceleration(1.5, FVector(0.0, 0.0, Gravity + 8.0))
	};
	TestTrue(TEXT("Normalized acceleration is accepted"),
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			FOpenMobileSensorsBackendRegistry::CaptureToken(),
			Backend.GetLastStartedPhysicalHandle(),
			Batch
		));
	FOpenMobileVectorSensorBatch Events;
	FOpenMobileSensorBufferReadResult Read;
	TestTrue(TEXT("The derived shake can be drained"),
		FOpenMobileSensorsSampleService::DrainBufferedVector(
			Owner,
			Shake.Handle,
			8,
			Read,
			Events
		));
	TestEqual(TEXT("Acceleration emits one shake"), Events.Samples.Num(), 1);
	if (Events.Samples.Num() == 1)
	{
		TestEqual(TEXT("The event identifies acceleration as its source"),
			Events.Samples[0].ShakeEvent.SourceSensor.Type,
			EOpenMobileSensorType::Accelerometer);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShakeRecordedFixtureTest,
	"OpenMobile.Sensors.Shake.RecordedFixture",
	EAutomationTestFlags_ApplicationContextMask |
		EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShakeRecordedFixtureTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsShakeTestsPrivate;
	const TSharedPtr<IPlugin> Plugin =
		IPluginManager::Get().FindPlugin(TEXT("OpenMobileSensors"));
	TestTrue(TEXT("The Sensors plugin is discoverable"), Plugin.IsValid());
	if (!Plugin.IsValid())
	{
		return false;
	}
	const FString FixturePath = FPaths::Combine(
		Plugin->GetBaseDir(),
		TEXT("Tests/Fixtures/ShakeLinearAcceleration.csv")
	);
	FString Fixture;
	TestTrue(TEXT("The recorded shake fixture can be loaded"),
		FFileHelper::LoadFileToString(Fixture, *FixturePath));
	if (Fixture.IsEmpty())
	{
		return false;
	}
	TArray<FString> Lines;
	Fixture.ParseIntoArrayLines(Lines, true);
	FString ActiveTrace;
	FOpenMobileSensorShakeDetector Detector;
	Detector.Configure(MakeConfig());
	int32 EventCount = 0;
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		TArray<FString> Fields;
		Lines[LineIndex].ParseIntoArray(Fields, TEXT(","), false);
		if (Fields.Num() != 6)
		{
			AddError(FString::Printf(
				TEXT("Fixture row %d has %d fields"),
				LineIndex + 1,
				Fields.Num()
			));
			continue;
		}
		if (Fields[0] != ActiveTrace)
		{
			ActiveTrace = Fields[0];
			Detector.Configure(MakeConfig());
		}
		const FOpenMobileVectorSensorSample Sample = MakeLinearAcceleration(
			FCString::Atod(*Fields[1]),
			FVector(
				FCString::Atod(*Fields[2]),
				FCString::Atod(*Fields[3]),
				FCString::Atod(*Fields[4])
			)
		);
		FOpenMobileVectorSensorSample Event;
		const bool bEmitted = Detector.Process(Sample, Event);
		const bool bExpected = FCString::Atoi(*Fields[5]) != 0;
		TestEqual(
			*FString::Printf(
				TEXT("Fixture row %d has the expected result"),
				LineIndex + 1
			),
			bEmitted,
			bExpected
		);
		EventCount += bEmitted ? 1 : 0;
	}
	TestEqual(TEXT("Only the real shake trace emits"), EventCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShakeDiscontinuityTest,
	"OpenMobile.Sensors.Shake.Discontinuities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShakeDiscontinuityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsShakeTestsPrivate;
	FOpenMobileSensorShakeDetector Detector;
	Detector.Configure(MakeConfig(2));
	FOpenMobileVectorSensorSample Event;
	TestFalse(TEXT("The first impulse starts the window"),
		Detector.Process(
			MakeLinearAcceleration(0.0, FVector(6.0, 0.0, 0.0)),
			Event
		));
	Detector.Process(MakeLinearAcceleration(0.05, FVector::ZeroVector), Event);
	Detector.Process(MakeLinearAcceleration(0.11, FVector::ZeroVector), Event);
	FOpenMobileVectorSensorSample ChangedSource = MakeLinearAcceleration(
		0.2,
		FVector(0.0, 6.0, 0.0)
	);
	ChangedSource.Header.SourceFlags |= static_cast<int32>(
		EOpenMobileSensorSourceFlags::Replay
	);
	TestFalse(TEXT("A source change clears old impulses"),
		Detector.Process(ChangedSource, Event));
	ChangedSource.Header.TimestampSeconds = 0.25;
	ChangedSource.Value = FVector::ZeroVector;
	Detector.Process(ChangedSource, Event);
	ChangedSource.Header.TimestampSeconds = 0.31;
	Detector.Process(ChangedSource, Event);
	ChangedSource.Header.TimestampSeconds = 0.4;
	ChangedSource.Value = FVector(0.0, 0.0, 6.0);
	TestTrue(TEXT("The new source builds independent state"),
		Detector.Process(ChangedSource, Event));

	Detector.Configure(MakeConfig(2));
	Detector.Process(
		MakeLinearAcceleration(1.0, FVector(6.0, 0.0, 0.0)),
		Event
	);
	TestFalse(TEXT("A long gap clears the partial window"),
		Detector.Process(
			MakeLinearAcceleration(7.0, FVector(0.0, 6.0, 0.0)),
			Event
		));
	Detector.Process(MakeLinearAcceleration(7.05, FVector::ZeroVector), Event);
	Detector.Process(MakeLinearAcceleration(7.11, FVector::ZeroVector), Event);
	TestTrue(TEXT("Detection resumes after the gap"),
		Detector.Process(
			MakeLinearAcceleration(7.2, FVector(0.0, 0.0, 6.0)),
			Event
		));
	return true;
}

#endif

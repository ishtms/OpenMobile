#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsAccuracyTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Magnetometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.MaximumCallbackFrequencyHz = 60.0;
		Request.Options.DeliveryMode = DeliveryMode;
		Request.Options.BufferCapacitySamples = 16;
		return Request;
	}

	FOpenMobileSensorSubscriptionResult StartActive(
		const FGuid& Owner,
		const FOpenMobileSensorSubscriptionRequest& Request
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				Request
			);
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		return Result;
	}

	FOpenMobileVectorSensorSample MakeVector(
		const FOpenMobileSensorIdentifier& Sensor,
		double TimestampSeconds,
		const FVector& Value,
		EOpenMobileSensorAccuracy Accuracy =
			EOpenMobileSensorAccuracy::Unknown
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.Accuracy = Accuracy;
		Sample.Value = Value;
		return Sample;
	}

	FOpenMobileSensorAccuracySnapshot MakeAccuracy(
		const FOpenMobileSensorIdentifier& Sensor,
		EOpenMobileSensorAccuracy Accuracy,
		double TimestampSeconds,
		bool bCalibrationRequired = false,
		TOptional<double> EstimatedError = {}
	)
	{
		FOpenMobileSensorAccuracySnapshot Snapshot;
		Snapshot.Sensor = Sensor;
		Snapshot.Accuracy = Accuracy;
		Snapshot.TimestampSeconds = TimestampSeconds;
		Snapshot.bCalibrationRequired = bCalibrationRequired;
		Snapshot.bHasEstimatedError = EstimatedError.IsSet();
		Snapshot.EstimatedError = EstimatedError.Get(0.0);
		return Snapshot;
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
	FOpenMobileSensorsAccuracyPlatformMappingsTest,
	"OpenMobile.Sensors.Accuracy.PlatformMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccuracyPlatformMappingsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAccuracyTestsPrivate;
	const FOpenMobileSensorIdentifier Sensor = MakeRequest().Sensor;
	TestEqual(TEXT("Unknown Android values stay unknown"),
		FOpenMobileSensorAccuracyMapper::FromAndroidAccuracyCallback(
			Sensor, 99, 1.0).Accuracy,
		EOpenMobileSensorAccuracy::Unknown);
	TestEqual(TEXT("Android unreliable maps exactly"),
		FOpenMobileSensorAccuracyMapper::FromAndroidAccuracyCallback(
			Sensor, 0, 1.0).Accuracy,
		EOpenMobileSensorAccuracy::Unreliable);
	TestEqual(TEXT("Android low maps exactly"),
		FOpenMobileSensorAccuracyMapper::FromAndroidAccuracyCallback(
			Sensor, 1, 1.0).Accuracy,
		EOpenMobileSensorAccuracy::Low);
	TestEqual(TEXT("Android medium maps exactly"),
		FOpenMobileSensorAccuracyMapper::FromAndroidAccuracyCallback(
			Sensor, 2, 1.0).Accuracy,
		EOpenMobileSensorAccuracy::Medium);
	const FOpenMobileSensorAccuracySnapshot AndroidHigh =
		FOpenMobileSensorAccuracyMapper::FromAndroidAccuracyCallback(
			Sensor, 3, 1.0);
	TestEqual(TEXT("Android high maps exactly"), AndroidHigh.Accuracy,
		EOpenMobileSensorAccuracy::High);
	TestFalse(TEXT("Android does not invent an error estimate"),
		AndroidHigh.bHasEstimatedError);
	TestFalse(TEXT("Android quality does not imply calibration"),
		AndroidHigh.bCalibrationRequired);

	const FOpenMobileSensorAccuracySnapshot IOSUncalibrated =
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor, -1, 2.0);
	TestEqual(TEXT("iOS uncalibrated field is unreliable"),
		IOSUncalibrated.Accuracy,
		EOpenMobileSensorAccuracy::Unreliable);
	TestTrue(TEXT("iOS uncalibrated field requests calibration"),
		IOSUncalibrated.bCalibrationRequired);
	TestEqual(TEXT("iOS magnetic low maps exactly"),
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor, 0, 2.0).Accuracy,
		EOpenMobileSensorAccuracy::Low);
	TestEqual(TEXT("iOS magnetic medium maps exactly"),
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor, 1, 2.0).Accuracy,
		EOpenMobileSensorAccuracy::Medium);
	TestEqual(TEXT("iOS magnetic high maps exactly"),
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor, 2, 2.0).Accuracy,
		EOpenMobileSensorAccuracy::High);

	const FOpenMobileSensorAccuracySnapshot IOSHeading =
		FOpenMobileSensorAccuracyMapper::FromIOSHeadingAccuracy(
			Sensor, 7.5, false, 3.0);
	TestEqual(TEXT("Heading error does not invent quality"),
		IOSHeading.Accuracy, EOpenMobileSensorAccuracy::Unknown);
	TestTrue(TEXT("Heading exposes the supplied error estimate"),
		IOSHeading.bHasEstimatedError);
	TestEqual(TEXT("Heading keeps the supplied degree error"),
		IOSHeading.EstimatedError, 7.5);
	const FOpenMobileSensorAccuracySnapshot IOSInvalidHeading =
		FOpenMobileSensorAccuracyMapper::FromIOSHeadingAccuracy(
			Sensor, -1.0, true, 4.0);
	TestEqual(TEXT("An invalid native heading is unreliable"),
		IOSInvalidHeading.Accuracy,
		EOpenMobileSensorAccuracy::Unreliable);
	TestFalse(TEXT("A negative heading error is not exposed"),
		IOSInvalidHeading.bHasEstimatedError);
	TestTrue(TEXT("The native calibration request is preserved"),
		IOSInvalidHeading.bCalibrationRequired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccuracyQualityTransitionsAndDeduplicationTest,
	"OpenMobile.Sensors.Accuracy.QualityTransitionsAndDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccuracyQualityTransitionsAndDeduplicationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAccuracyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AccuracyTransitions"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	TArray<FOpenMobileSensorAccuracySnapshot> Changes;
	FOpenMobileSensorsSampleService::OnAccuracyChanged().AddLambda(
		[&](
			const FGuid& EventOwner,
			const FOpenMobileSensorSubscriptionHandle& EventHandle,
			const FOpenMobileSensorAccuracySnapshot& Snapshot
		)
		{
			if (EventOwner == Owner && EventHandle == Subscription.Handle)
			{
				Changes.Add(Snapshot);
			}
		}
	);
	FOpenMobileSensorsSampleService::PublishAccuracy(
		MakeAccuracy(Request.Sensor, EOpenMobileSensorAccuracy::Unknown, 1.0));
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::Unknown,
		2.0,
		false,
		12.0
	));
	FOpenMobileSensorsSampleService::PublishAccuracy(
		MakeAccuracy(Request.Sensor, EOpenMobileSensorAccuracy::Low, 3.0));
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor, EOpenMobileSensorAccuracy::Low, 4.0, true));
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor, EOpenMobileSensorAccuracy::Low, 5.0, true, 9.0));
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor, EOpenMobileSensorAccuracy::High, 6.0, false, 2.5));
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor, EOpenMobileSensorAccuracy::High, 6.5, false, 1.5));
	FOpenMobileSensorsSampleService::PublishAccuracy(
		MakeAccuracy(Request.Sensor, EOpenMobileSensorAccuracy::Low, 5.5));
	TestEqual(TEXT("Worker-side reports do not invoke public callbacks"),
		Changes.Num(), 0);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(7.0);
	TestEqual(TEXT("Only initial and material changes are broadcast"),
		Changes.Num(), 4);
	if (Changes.Num() == 4)
	{
		TestEqual(TEXT("The initial missing state is explicit"),
			Changes[0].Accuracy, EOpenMobileSensorAccuracy::Unknown);
		TestEqual(TEXT("Quality transition is retained"),
			Changes[1].Accuracy, EOpenMobileSensorAccuracy::Low);
		TestTrue(TEXT("Calibration transition is retained"),
			Changes[2].bCalibrationRequired);
		TestEqual(TEXT("Recovery reaches high quality"),
			Changes[3].Accuracy, EOpenMobileSensorAccuracy::High);
		for (int32 Index = 0; Index < Changes.Num(); ++Index)
		{
			TestEqual(TEXT("Change sequences are contiguous"),
				Changes[Index].Sequence, static_cast<int64>(Index + 1));
		}
	}
	FOpenMobileSensorsSampleService::PublishVector(
		MakeVector(Request.Sensor, 7.0, FVector(1.0, 2.0, 3.0)));
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Sample;
	TestTrue(TEXT("A sample is available after accuracy recovery"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner, Subscription.Handle, 0, 7.0, Read, Sample));
	TestEqual(TEXT("The latest normalized quality reaches every sample"),
		Sample.Header.Accuracy, EOpenMobileSensorAccuracy::High);
	TestFalse(TEXT("The recovered sample clears calibration"),
		Sample.Header.bCalibrationRequired);
	TestTrue(TEXT("The latest supplied error reaches the sample"),
		Sample.Header.bHasEstimatedError);
	TestEqual(TEXT("Error-only updates refresh sample context"),
		Sample.Header.EstimatedError, 1.5);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccuracyMissingAccuracyTest,
	"OpenMobile.Sensors.Accuracy.MissingAccuracy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccuracyMissingAccuracyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAccuracyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("MissingAccuracy"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	int32 ChangeCount = 0;
	FOpenMobileSensorsSampleService::OnAccuracyChanged().AddLambda(
		[&](const FGuid&, const FOpenMobileSensorSubscriptionHandle&,
			const FOpenMobileSensorAccuracySnapshot&)
		{
			++ChangeCount;
		}
	);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeVector(Request.Sensor, 1.0, FVector::ForwardVector));
	FOpenMobileSensorsSampleService::PublishVector(
		MakeVector(Request.Sensor, 2.0, FVector::RightVector));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(2.0);
	TestEqual(TEXT("Repeated missing accuracy is deduplicated"),
		ChangeCount, 1);
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Sample;
	FOpenMobileSensorsSampleService::ReadLatestVector(
		Owner, Subscription.Handle, 0, 2.0, Read, Sample);
	TestEqual(TEXT("Missing native accuracy remains unknown"),
		Sample.Header.Accuracy, EOpenMobileSensorAccuracy::Unknown);
	TestFalse(TEXT("Missing accuracy has no invented error"),
		Sample.Header.bHasEstimatedError);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccuracyInvalidValuesAndRecoveryTest,
	"OpenMobile.Sensors.Accuracy.InvalidValuesAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccuracyInvalidValuesAndRecoveryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAccuracyTestsPrivate;
	FOpenMobileVectorSensorSample Native = MakeVector(
		MakeRequest().Sensor,
		1.0,
		FVector(
			1.0,
			std::numeric_limits<double>::quiet_NaN(),
			3.0
		)
	);
	Native.Header.bUnitsNormalized = false;
	TestFalse(TEXT("Nonfinite native input is invalid"),
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(
			EOpenMobileSensorNativePlatform::Android, Native));
	TestFalse(TEXT("The invalid sample is marked"), Native.Header.bValid);
	TestTrue(TEXT("The invalid component is not replaced with zero"),
		FMath::IsNaN(Native.Value.Y));

	FOpenMobileHeadingSensorSample Heading;
	Heading.Header.bValid = true;
	Heading.HeadingDegrees = 45.0;
	Heading.bHasAccuracyDegrees = true;
	Heading.AccuracyDegrees = std::numeric_limits<double>::quiet_NaN();
	FOpenMobileSensorUnitConverter::NormalizeHeadingSample(
		EOpenMobileSensorNativePlatform::IOS, Heading);
	TestFalse(TEXT("Invalid optional heading accuracy is removed"),
		Heading.bHasAccuracyDegrees);
	TestTrue(TEXT("Invalid optional metadata is not rewritten as zero"),
		FMath::IsNaN(Heading.AccuracyDegrees));

	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("InvalidRecovery"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorDeliveryMode::EventBatches);
	const FOpenMobileSensorSubscriptionResult Subscription =
		StartActive(Owner, Request);
	int32 DeliveredSampleCount = 0;
	FOpenMobileSensorsSampleService::OnVectorBatch().AddLambda(
		[&](const FGuid&, const FOpenMobileSensorSubscriptionHandle&,
			const FOpenMobileVectorSensorBatch& Batch)
		{
			DeliveredSampleCount += Batch.Samples.Num();
		}
	);
	FOpenMobileVectorSensorSample Invalid = MakeVector(
		Request.Sensor,
		2.0,
		FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0),
		EOpenMobileSensorAccuracy::Unreliable
	);
	FOpenMobileSensorsSampleService::PublishVector(Invalid);
	FOpenMobileSensorsSampleService::PublishVector(MakeVector(
		Request.Sensor,
		3.0,
		FVector(5.0, 0.0, 0.0),
		EOpenMobileSensorAccuracy::High
	));
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Recovered;
	TestTrue(TEXT("The valid recovery sample becomes readable"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner, Subscription.Handle, 0, 3.0, Read, Recovered));
	TestEqual(TEXT("Invalid input never substitutes a zero snapshot"),
		Recovered.Value.X, 5.0);
	TestEqual(TEXT("Invalid input does not consume a sample sequence"),
		Recovered.Header.Sequence, 1ll);
	TestTrue(TEXT("Recovery resets future stateful processing"),
		Recovered.Header.bStatefulProcessingReset);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(3.0);
	TestEqual(TEXT("Invalid input is excluded from downstream delivery"),
		DeliveredSampleCount, 1);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccuracyBackendGenerationTest,
	"OpenMobile.Sensors.Accuracy.BackendGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccuracyBackendGenerationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAccuracyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend OldBackend(TEXT("OldAccuracy"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(OldBackend);
	const FOpenMobileSensorsBackendToken OldToken =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FOpenMobileSensorBackendStreamHandle OldPhysicalHandle =
		{FGuid::NewGuid()};
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(OldBackend);

	FOpenMobileSensorsMockBackend Backend(TEXT("CurrentAccuracy"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	StartActive(Owner, Request);
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	const FOpenMobileSensorAccuracySnapshot Report = MakeAccuracy(
		Request.Sensor, EOpenMobileSensorAccuracy::Medium, 1.0);
	TestFalse(TEXT("A stale backend generation cannot report accuracy"),
		FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
			OldToken, OldPhysicalHandle, Report));
	TestFalse(TEXT("A stale physical stream cannot report accuracy"),
		FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
			Token, OldPhysicalHandle, Report));
	TFuture<bool> CurrentReport = Async(EAsyncExecution::ThreadPool, [&]()
	{
		return FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
			Token, PhysicalHandle, Report);
	});
	TestTrue(TEXT("The current worker callback can report accuracy"),
		CurrentReport.Get());
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccuracySubsystemEventTest,
	"OpenMobile.Sensors.Accuracy.SubsystemEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccuracySubsystemEventTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAccuracyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AccuracySubsystem"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	int32 CallbackCount = 0;
	FOpenMobileSensorSubscriptionHandle ReceivedHandle;
	FOpenMobileSensorAccuracySnapshot ReceivedSnapshot;
	Subsystem->OnAccuracyChangedNative().AddLambda(
		[&](
			FOpenMobileSensorSubscriptionHandle Handle,
			const FOpenMobileSensorAccuracySnapshot& Snapshot
		)
		{
			++CallbackCount;
			ReceivedHandle = Handle;
			ReceivedSnapshot = Snapshot;
		}
	);
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		Subsystem->StartSubscriptionNative(Request);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor, EOpenMobileSensorAccuracy::Medium, 1.0));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.0);
	TestEqual(TEXT("The subsystem broadcasts one normalized change"),
		CallbackCount, 1);
	TestEqual(TEXT("The event identifies its logical handle"),
		ReceivedHandle, Subscription.Handle);
	TestEqual(TEXT("The event carries normalized quality"),
		ReceivedSnapshot.Accuracy, EOpenMobileSensorAccuracy::Medium);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

#endif

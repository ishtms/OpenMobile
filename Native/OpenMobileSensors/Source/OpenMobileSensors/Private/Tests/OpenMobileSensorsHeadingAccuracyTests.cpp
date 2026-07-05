#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorHeadingQuality.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsHeadingAccuracyTestsPrivate
{
	FOpenMobileHeadingSensorSample MakeHeading(
		double TimestampSeconds,
		EOpenMobileSensorAccuracy Accuracy,
		bool bCalibrationRequired = false
	)
	{
		FOpenMobileHeadingSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::MagneticHeading;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.Accuracy = Accuracy;
		Sample.Header.bCalibrationRequired = bCalibrationRequired;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::NativeFused
		) | static_cast<int32>(
			EOpenMobileSensorSourceFlags::MagneticNorthReferenced
		);
		Sample.HeadingDegrees = 45.0;
		Sample.Reference = EOpenMobileHeadingReference::MagneticNorth;
		Sample.bTiltCompensated = true;
		return Sample;
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
	FOpenMobileSensorsHeadingQualityCombinationTest,
	"OpenMobile.Sensors.Heading.Accuracy.QualityCombination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHeadingQualityCombinationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsHeadingAccuracyTestsPrivate;
	FOpenMobileHeadingSensorSample MissingEstimate = MakeHeading(
		1.0,
		EOpenMobileSensorAccuracy::Unknown
	);
	FOpenMobileSensorHeadingQuality::Normalize(MissingEstimate);
	TestEqual(TEXT("Absent quality stays unknown"),
		MissingEstimate.Header.Accuracy,
		EOpenMobileSensorAccuracy::Unknown);
	TestFalse(TEXT("Absent estimate is not invented"),
		MissingEstimate.bHasAccuracyDegrees);
	TestFalse(TEXT("Header estimate also stays absent"),
		MissingEstimate.Header.bHasEstimatedError);

	FOpenMobileHeadingSensorSample DegradedAttitude = MakeHeading(
		2.0,
		EOpenMobileSensorAccuracy::High
	);
	DegradedAttitude.Header.Fusion.Quality =
		EOpenMobileSensorFusionQuality::Degraded;
	FOpenMobileSensorHeadingQuality::Normalize(DegradedAttitude);
	TestEqual(TEXT("Degraded attitude caps heading quality"),
		DegradedAttitude.Header.Accuracy,
		EOpenMobileSensorAccuracy::Low);

	FOpenMobileHeadingSensorSample AgingLocation = MakeHeading(
		3.0,
		EOpenMobileSensorAccuracy::High
	);
	AgingLocation.Header.Sensor.Type = EOpenMobileSensorType::TrueHeading;
	AgingLocation.Reference = EOpenMobileHeadingReference::TrueNorth;
	AgingLocation.bHasLocationAgeSeconds = true;
	AgingLocation.LocationAgeSeconds = 45.0;
	FOpenMobileSensorHeadingQuality::Normalize(AgingLocation);
	TestEqual(TEXT("Aging location caps true-heading quality"),
		AgingLocation.Header.Accuracy,
		EOpenMobileSensorAccuracy::Medium);

	FOpenMobileHeadingSensorSample Calibration = MakeHeading(
		4.0,
		EOpenMobileSensorAccuracy::High,
		true
	);
	FOpenMobileSensorHeadingQuality::Normalize(Calibration);
	TestEqual(TEXT("Calibration requirement is never high quality"),
		Calibration.Header.Accuracy,
		EOpenMobileSensorAccuracy::Unreliable);

	const FOpenMobileSensorAccuracySnapshot NegativeSentinel =
		FOpenMobileSensorAccuracyMapper::FromIOSHeadingAccuracy(
			Calibration.Header.Sensor,
			-1.0,
			false,
			5.0
		);
	TestEqual(TEXT("Negative native sentinel is unreliable"),
		NegativeSentinel.Accuracy,
		EOpenMobileSensorAccuracy::Unreliable);
	TestFalse(TEXT("Negative native sentinel has no estimate"),
		NegativeSentinel.bHasEstimatedError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsHeadingMinimumCallbackAccuracyTest,
	"OpenMobile.Sensors.Heading.Accuracy.MinimumCallbackQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHeadingMinimumCallbackAccuracyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsHeadingAccuracyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("HeadingAccuracyThreshold"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::MagneticHeading;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	Request.Options.CustomFrequencyHz = 60.0;
	Request.Options.MaximumCallbackFrequencyHz = 60.0;
	Request.Options.DeliveryMode = EOpenMobileSensorDeliveryMode::EventBatches;
	Request.Options.MinimumCallbackAccuracy =
		EOpenMobileSensorAccuracy::Medium;
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	int32 DeliveredSamples = 0;
	EOpenMobileSensorAccuracy DeliveredAccuracy =
		EOpenMobileSensorAccuracy::Unknown;
	FOpenMobileSensorsSampleService::OnHeadingBatch().AddLambda(
		[&](
			const FGuid& EventOwner,
			const FOpenMobileSensorSubscriptionHandle& EventHandle,
			const FOpenMobileHeadingSensorBatch& Batch
		)
		{
			if (EventOwner == Owner && EventHandle == Subscription.Handle)
			{
				DeliveredSamples += Batch.Samples.Num();
				if (!Batch.Samples.IsEmpty())
				{
					DeliveredAccuracy = Batch.Samples.Last().Header.Accuracy;
				}
			}
		}
	);

	FOpenMobileSensorsSampleService::PublishHeading(
		MakeHeading(1.0, EOpenMobileSensorAccuracy::Low, true)
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileHeadingSensorSample Latest;
	TestTrue(TEXT("Low-quality heading remains available to polling"),
		FOpenMobileSensorsSampleService::ReadLatestHeading(
			Owner,
			Subscription.Handle,
			0,
			1.1,
			Read,
			Latest
		));
	TestEqual(TEXT("Polling retains unreliable calibration state"),
		Latest.Header.Accuracy,
		EOpenMobileSensorAccuracy::Unreliable);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.1);
	TestEqual(TEXT("Low-quality callback is withheld"), DeliveredSamples, 0);

	FOpenMobileSensorsSampleService::PublishHeading(
		MakeHeading(2.0, EOpenMobileSensorAccuracy::High)
	);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(2.1);
	TestEqual(TEXT("Recovered heading reaches callback"),
		DeliveredSamples,
		1);
	TestEqual(TEXT("Callback reports recovered quality"),
		DeliveredAccuracy,
		EOpenMobileSensorAccuracy::High);
	FinishBackend(Backend);
	return true;
}

#endif

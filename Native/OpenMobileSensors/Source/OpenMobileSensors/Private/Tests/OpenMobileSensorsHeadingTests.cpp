#include "Misc/AutomationTest.h"

#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorDeclination.h"
#include "OpenMobileSensorHeading.h"
#include "OpenMobileSensorHeadingFilter.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorsTrueHeadingService.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorsSubscriptionService.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsWMM2025DeclinationFixturesTest,
	"OpenMobile.Sensors.Heading.True.WMM2025Fixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsWMM2025DeclinationFixturesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	struct FFixture
	{
		double DecimalYear;
		double AltitudeMeters;
		double LatitudeDegrees;
		double LongitudeDegrees;
		double ExpectedDeclinationDegrees;
		double ExpectedHorizontalFieldNanoTesla;
	};
	const FFixture Fixtures[] = {
		{2025.0, 0.0, 80.0, 0.0, 1.28, 6523.2},
		{2025.0, 0.0, 0.0, 120.0, -0.16, 39677.9},
		{2025.0, 0.0, -80.0, 240.0, 68.78, 16898.1},
		{2025.0, 100000.0, 80.0, 0.0, 0.85, 6216.7},
		{2027.5, 0.0, 80.0, 0.0, 2.59, 6507.5},
		{2027.5, 100000.0, -80.0, 240.0, 67.93, 15927.0}
	};
	for (const FFixture& Fixture : Fixtures)
	{
		FOpenMobileSensorDeclinationResult Result;
		TestTrue(TEXT("Published WMM2025 fixture is calculated"),
			FOpenMobileSensorDeclination::CalculateWMM2025(
				Fixture.LatitudeDegrees,
				Fixture.LongitudeDegrees,
				Fixture.AltitudeMeters,
				Fixture.DecimalYear,
				Result
			));
		TestTrue(TEXT("Declination matches the published fixture"),
			FMath::IsNearlyEqual(
				Result.DeclinationDegrees,
				Fixture.ExpectedDeclinationDegrees,
				0.01
			));
		TestTrue(TEXT("Horizontal intensity matches the published fixture"),
			FMath::IsNearlyEqual(
				Result.HorizontalFieldNanoTesla,
				Fixture.ExpectedHorizontalFieldNanoTesla,
				0.1
			));
		TestTrue(TEXT("Estimated declination error is finite and positive"),
			FMath::IsFinite(Result.EstimatedErrorDegrees)
				&& Result.EstimatedErrorDegrees > 0.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTrueHeadingLocationValidityTest,
	"OpenMobile.Sensors.Heading.True.LocationValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTrueHeadingLocationValidityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	const FGuid Owner = FGuid::NewGuid();
	const FGuid OtherOwner = FGuid::NewGuid();
	constexpr double CurrentUnixSeconds = 1735689600.0;
	constexpr double CurrentMonotonicSeconds = 100.0;
	FOpenMobileSensorLocationInput Location;
	Location.LatitudeDegrees = 80.0;
	Location.LongitudeDegrees = 0.0;
	Location.AltitudeMeters = 0.0;
	Location.HorizontalAccuracyMeters = 10.0;
	Location.TimestampSeconds = CurrentUnixSeconds;
	double LocationAgeSeconds = -1.0;
	FOpenMobileSensorLocationInput ResolvedLocation;
	TestEqual(TEXT("Absent owner input is unavailable"),
		FOpenMobileSensorsTrueHeadingService::GetUsableLocationInput(
			Owner,
			CurrentMonotonicSeconds,
			ResolvedLocation,
			LocationAgeSeconds
		),
		EOpenMobileSensorFailureReason::MissingLocationInput);

	FOpenMobileSensorLocationInput StaleLocation = Location;
	StaleLocation.TimestampSeconds -= 61.0;
	TestEqual(TEXT("Stale input is rejected when supplied"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			StaleLocation,
			CurrentUnixSeconds,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::StaleLocationInput);

	FOpenMobileSensorLocationInput InaccurateLocation = Location;
	InaccurateLocation.HorizontalAccuracyMeters = 101.0;
	TestEqual(TEXT("Inaccurate input is rejected"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			InaccurateLocation,
			CurrentUnixSeconds,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::PoorLocationAccuracy);

	FOpenMobileSensorLocationInput FutureLocation = Location;
	FutureLocation.TimestampSeconds += 6.0;
	TestEqual(TEXT("Implausibly future input is invalid"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			FutureLocation,
			CurrentUnixSeconds,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::InvalidRequest);

	FOpenMobileSensorLocationInput ExpiredModelLocation = Location;
	ExpiredModelLocation.TimestampSeconds = 1893456000.0;
	TestEqual(TEXT("Location outside the model date is unavailable"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			ExpiredModelLocation,
			1893456000.0,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::DerivedInputUnavailable);

	TestEqual(TEXT("Recent accurate input is accepted"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			Location,
			CurrentUnixSeconds,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::None);
	TestEqual(TEXT("Another owner cannot use the input"),
		FOpenMobileSensorsTrueHeadingService::GetUsableLocationInput(
			OtherOwner,
			CurrentMonotonicSeconds,
			ResolvedLocation,
			LocationAgeSeconds
		),
		EOpenMobileSensorFailureReason::MissingLocationInput);
	TestEqual(TEXT("Owner receives usable input"),
		FOpenMobileSensorsTrueHeadingService::GetUsableLocationInput(
			Owner,
			CurrentMonotonicSeconds + 10.0,
			ResolvedLocation,
			LocationAgeSeconds
		),
		EOpenMobileSensorFailureReason::None);
	TestEqual(TEXT("Location age advances monotonically"),
		LocationAgeSeconds,
		10.0);
	TestEqual(TEXT("Stored position is preserved"),
		ResolvedLocation.LatitudeDegrees,
		Location.LatitudeDegrees);
	TestEqual(TEXT("Stored input eventually expires"),
		FOpenMobileSensorsTrueHeadingService::GetUsableLocationInput(
			Owner,
			CurrentMonotonicSeconds + 61.0,
			ResolvedLocation,
			LocationAgeSeconds
		),
		EOpenMobileSensorFailureReason::StaleLocationInput);
	TestFalse(TEXT("Expired input no longer satisfies capability state"),
		FOpenMobileSensorsTrueHeadingService::HasAnyLocationInput(
			CurrentMonotonicSeconds + 61.0
		));
	FOpenMobileSensorsTrueHeadingService::RemoveOwner(Owner);
	TestEqual(TEXT("Removed owner input is unavailable"),
		FOpenMobileSensorsTrueHeadingService::GetUsableLocationInput(
			Owner,
			CurrentMonotonicSeconds,
			ResolvedLocation,
			LocationAgeSeconds
		),
		EOpenMobileSensorFailureReason::MissingLocationInput);
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTrueHeadingConversionTest,
	"OpenMobile.Sensors.Heading.True.ConversionMetadataAndWraparound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTrueHeadingConversionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	const FGuid Owner = FGuid::NewGuid();
	constexpr double CurrentUnixSeconds = 1735689600.0;
	constexpr double CurrentMonotonicSeconds = 100.0;
	FOpenMobileSensorLocationInput Location;
	Location.LatitudeDegrees = 80.0;
	Location.LongitudeDegrees = 0.0;
	Location.HorizontalAccuracyMeters = 10.0;
	Location.TimestampSeconds = CurrentUnixSeconds;
	TestEqual(TEXT("Fixture location is accepted"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			Location,
			CurrentUnixSeconds,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::None);

	FOpenMobileHeadingSensorSample Magnetic;
	Magnetic.Header.Sensor.Type = EOpenMobileSensorType::MagneticHeading;
	Magnetic.Header.Sensor.InstanceId = TEXT("Default");
	Magnetic.Header.bValid = true;
	Magnetic.Header.SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::NativeFused
	) | static_cast<int32>(
		EOpenMobileSensorSourceFlags::MagneticNorthReferenced
	);
	Magnetic.HeadingDegrees = 359.0;
	Magnetic.Reference = EOpenMobileHeadingReference::MagneticNorth;
	Magnetic.bTiltCompensated = true;
	Magnetic.bHasAccuracyDegrees = true;
	Magnetic.AccuracyDegrees = 2.0;
	FOpenMobileHeadingSensorSample TrueHeading;
	TestEqual(TEXT("Magnetic heading converts to true heading"),
		FOpenMobileSensorsTrueHeadingService::ConvertMagneticHeading(
			Owner,
			CurrentMonotonicSeconds + 10.0,
			Magnetic,
			TrueHeading
		),
		EOpenMobileSensorFailureReason::None);
	TestTrue(TEXT("Declination wraps into the heading range"),
		FMath::IsNearlyEqual(TrueHeading.HeadingDegrees, 0.28, 0.01));
	TestEqual(TEXT("Converted sample is true north referenced"),
		TrueHeading.Reference,
		EOpenMobileHeadingReference::TrueNorth);
	TestEqual(TEXT("Converted sensor identity is true heading"),
		TrueHeading.Header.Sensor.Type,
		EOpenMobileSensorType::TrueHeading);
	TestEqual(TEXT("WMM2025 is reported as the declination source"),
		TrueHeading.DeclinationSource,
		EOpenMobileHeadingDeclinationSource::WorldMagneticModel2025);
	TestTrue(TEXT("Declination value is reported"),
		TrueHeading.bHasDeclinationDegrees);
	TestTrue(TEXT("Reported declination matches the fixture"),
		FMath::IsNearlyEqual(TrueHeading.DeclinationDegrees, 1.28, 0.01));
	TestTrue(TEXT("Location age is reported"),
		TrueHeading.bHasLocationAgeSeconds);
	TestEqual(TEXT("Location age matches monotonic elapsed time"),
		TrueHeading.LocationAgeSeconds,
		10.0);
	TestTrue(TEXT("Estimated heading accuracy is reported"),
		TrueHeading.bHasAccuracyDegrees);
	TestTrue(TEXT("Model error is combined with sensor accuracy"),
		TrueHeading.AccuracyDegrees > Magnetic.AccuracyDegrees);
	TestTrue(TEXT("Converted source is marked plugin derived"),
		EnumHasAnyFlags(
			static_cast<EOpenMobileSensorSourceFlags>(
				TrueHeading.Header.SourceFlags
			),
			EOpenMobileSensorSourceFlags::PluginDerived
		));
	TestTrue(TEXT("Converted source is true north referenced"),
		EnumHasAnyFlags(
			static_cast<EOpenMobileSensorSourceFlags>(
				TrueHeading.Header.SourceFlags
			),
			EOpenMobileSensorSourceFlags::TrueNorthReferenced
		));
	TestFalse(TEXT("Converted source is not labeled magnetic north"),
		EnumHasAnyFlags(
			static_cast<EOpenMobileSensorSourceFlags>(
				TrueHeading.Header.SourceFlags
			),
			EOpenMobileSensorSourceFlags::MagneticNorthReferenced
		));
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeTrueHeadingMetadataTest,
	"OpenMobile.Sensors.Heading.True.NativePlatformMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeTrueHeadingMetadataTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	const FGuid Owner = FGuid::NewGuid();
	constexpr double CurrentUnixSeconds = 1735689600.0;
	constexpr double CurrentMonotonicSeconds = 100.0;
	FOpenMobileSensorLocationInput Location;
	Location.LatitudeDegrees = 80.0;
	Location.HorizontalAccuracyMeters = 10.0;
	Location.TimestampSeconds = CurrentUnixSeconds;
	FOpenMobileSensorsTrueHeadingService::SetLocationInput(
		Owner,
		Location,
		CurrentUnixSeconds,
		CurrentMonotonicSeconds
	);
	FOpenMobileHeadingSensorSample Native;
	Native.Header.Sensor.Type = EOpenMobileSensorType::TrueHeading;
	Native.Header.Sensor.InstanceId = TEXT("Default");
	Native.Header.bValid = true;
	Native.Header.SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::NativeFused
	) | static_cast<int32>(
		EOpenMobileSensorSourceFlags::TrueNorthReferenced
	);
	Native.HeadingDegrees = 42.0;
	Native.Reference = EOpenMobileHeadingReference::TrueNorth;
	TestEqual(TEXT("Native true heading accepts valid location context"),
		FOpenMobileSensorsTrueHeadingService::AnnotateNativeHeading(
			Owner,
			CurrentMonotonicSeconds + 5.0,
			Native
		),
		EOpenMobileSensorFailureReason::None);
	TestEqual(TEXT("Native platform is reported as declination source"),
		Native.DeclinationSource,
		EOpenMobileHeadingDeclinationSource::NativePlatform);
	TestFalse(TEXT("Native platform does not invent declination value"),
		Native.bHasDeclinationDegrees);
	TestTrue(TEXT("Native sample reports location age"),
		Native.bHasLocationAgeSeconds);
	TestEqual(TEXT("Native sample location age is current"),
		Native.LocationAgeSeconds,
		5.0);
	TestFalse(TEXT("Native platform source is not plugin derived"),
		EnumHasAnyFlags(
			static_cast<EOpenMobileSensorSourceFlags>(
				Native.Header.SourceFlags
			),
			EOpenMobileSensorSourceFlags::PluginDerived
		));
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTrueHeadingFallbackDeliveryTest,
	"OpenMobile.Sensors.Heading.True.FallbackDeliveryAndPermission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTrueHeadingFallbackDeliveryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	FOpenMobileSensorsSampleService::ResetForTests();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsTrueHeadingService::ResetForTests();

	FOpenMobileSensorCapability MagneticCapability;
	MagneticCapability.Sensor.Type = EOpenMobileSensorType::MagneticHeading;
	MagneticCapability.Sensor.InstanceId = TEXT("Default");
	MagneticCapability.Availability.Name =
		FOpenMobileSensorTypes::GetStableName(
			EOpenMobileSensorType::MagneticHeading
		);
	MagneticCapability.Availability.State =
		EOpenMobileCapabilityState::Available;
	MagneticCapability.Source =
		EOpenMobileSensorAvailabilitySource::Native;
	MagneticCapability.MinimumFrequencyHz = 1.0;
	MagneticCapability.MaximumFrequencyHz = 100.0;
	FOpenMobileSensorsMockBackend Backend(TEXT("TrueHeadingParity"));
	Backend.SetSensorCapabilities({MagneticCapability});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);

	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::TrueHeading;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	Request.Options.CustomFrequencyHz = 30.0;
	Request.Options.bAllowDerivedFallback = true;
	const FOpenMobileSensorSubscriptionResult MissingLocation =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	TestEqual(TEXT("Absent location blocks true heading"),
		MissingLocation.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::MissingLocationInput);

	const double CurrentUnixSeconds = static_cast<double>(
		FDateTime::UtcNow().ToUnixTimestamp()
	);
	const double CurrentMonotonicSeconds = FPlatformTime::Seconds();
	FOpenMobileSensorLocationInput Location;
	Location.LatitudeDegrees = 80.0;
	Location.LongitudeDegrees = 0.0;
	Location.HorizontalAccuracyMeters = 10.0;
	Location.TimestampSeconds = CurrentUnixSeconds;
	TestEqual(TEXT("Current test location is accepted"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			Location,
			CurrentUnixSeconds,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::None);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(true);
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner,
		EOpenMobileSensorScreenRotation::Rotation90,
		0.5,
		false
	);
	Request.Options.CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::CurrentScreen;
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	TestTrue(TEXT("Valid prerequisites accept true heading"),
		Subscription.Operation.IsSuccess());
	if (Subscription.Operation.IsSuccess())
	{
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		TestEqual(TEXT("Fallback opens magnetic heading on every platform"),
			Backend.GetLastStartedPhysicalRequest().Sensor.Type,
			EOpenMobileSensorType::MagneticHeading);

		FOpenMobileHeadingSensorSample Magnetic;
		Magnetic.Header.Sensor = MagneticCapability.Sensor;
		Magnetic.Header.TimestampSeconds = 1.0;
		Magnetic.Header.bValid = true;
		Magnetic.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::NativeFused
		) | static_cast<int32>(
			EOpenMobileSensorSourceFlags::MagneticNorthReferenced
		);
		Magnetic.HeadingDegrees = 359.0;
		Magnetic.Reference = EOpenMobileHeadingReference::MagneticNorth;
		Magnetic.bTiltCompensated = true;
		Magnetic.bHasAccuracyDegrees = true;
		Magnetic.AccuracyDegrees = 2.0;
		FOpenMobileHeadingSensorBatch Batch;
		Batch.Samples.Add(Magnetic);
		TestTrue(TEXT("Magnetic source batch is accepted"),
			FOpenMobileSensorsSampleService::PublishHeadingBatchFromBackend(
				FOpenMobileSensorsBackendRegistry::CaptureToken(),
				Backend.GetLastStartedPhysicalHandle(),
				Batch
			));
		FOpenMobileSensorReadResult Read;
		FOpenMobileHeadingSensorSample Derived;
		TestTrue(TEXT("Fallback publishes true heading"),
			FOpenMobileSensorsSampleService::ReadLatestHeading(
				Owner,
				Subscription.Handle,
				0,
				1.1,
				Read,
				Derived
			));
		TestEqual(TEXT("Delivered sample keeps true heading identity"),
			Derived.Header.Sensor.Type,
			EOpenMobileSensorType::TrueHeading);
		TestEqual(TEXT("Derived sample applies screen rotation once"),
			Derived.Header.ScreenRotation,
			EOpenMobileSensorScreenRotation::Rotation90);
		TestEqual(TEXT("Derived sample stays true north referenced"),
			Derived.Reference,
			EOpenMobileHeadingReference::TrueNorth);
		FOpenMobileSensorsSubscriptionService::StopSubscription(
			Owner,
			Subscription.Handle
		);
	}

	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::TrueHeadingLocation
		),
		EOpenMobilePermissionStatus::Denied
	);
	const FOpenMobileSensorSubscriptionResult Denied =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	TestEqual(TEXT("Denied location dependency blocks true heading"),
		Denied.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::PermissionDenied);

	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsSampleService::ResetForTests();
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAndroidMagneticHeadingFixturesTest,
	"OpenMobile.Sensors.Heading.Magnetic.AndroidRotationVectorFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAndroidMagneticHeadingFixturesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	struct FFixture
	{
		const TCHAR* Name;
		FQuat RotationVector;
		double ExpectedHeadingDegrees;
	};
	const FFixture Fixtures[] = {
		{TEXT("North"), FQuat(0.0, 0.0, 0.0, 1.0), 0.0},
		{TEXT("East"), FQuat(0.0, 0.0, -0.70710678, 0.70710678), 90.0},
		{TEXT("South"), FQuat(0.0, 0.0, -1.0, 0.0), 180.0},
		{TEXT("West"), FQuat(0.0, 0.0, -0.70710678, -0.70710678), 270.0},
		{TEXT("Wraparound"), FQuat(0.0, 0.0, -0.00872654, -0.99996192), 359.0},
		{TEXT("TiltedNorth"), FQuat(0.5, 0.0, 0.0, 0.8660254), 0.0},
		{TEXT("TiltedEast"), FQuat(0.35355339, -0.35355339, -0.61237244, 0.61237244), 90.0},
		{TEXT("TiltedSouth"), FQuat(0.0, -0.5, -0.8660254, 0.0), 180.0},
		{TEXT("TiltedWest"), FQuat(-0.35355339, -0.35355339, -0.61237244, -0.61237244), 270.0}
	};
	for (const FFixture& Fixture : Fixtures)
	{
		double HeadingDegrees = -1.0;
		TestTrue(FString::Printf(TEXT("%s fixture is valid"), Fixture.Name),
			FOpenMobileSensorHeading::FromAndroidRotationVector(
				Fixture.RotationVector,
				HeadingDegrees
			));
		TestTrue(FString::Printf(TEXT("%s heading matches"), Fixture.Name),
			FMath::IsNearlyEqual(
				HeadingDegrees,
				Fixture.ExpectedHeadingDegrees,
				1.e-4
			));
	}
	double IgnoredHeading = 0.0;
	TestFalse(TEXT("A zero rotation vector is rejected"),
		FOpenMobileSensorHeading::FromAndroidRotationVector(
			FQuat(0.0, 0.0, 0.0, 0.0),
			IgnoredHeading
		));
	TestFalse(TEXT("A vertical device with no horizontal top is rejected"),
		FOpenMobileSensorHeading::FromAndroidRotationVector(
			FQuat(0.70710678, 0.0, 0.0, 0.70710678),
			IgnoredHeading
		));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMagneticHeadingScreenRotationTest,
	"OpenMobile.Sensors.Heading.Magnetic.ScreenRotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMagneticHeadingScreenRotationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	struct FFixture
	{
		EOpenMobileSensorScreenRotation Rotation;
		double ExpectedHeadingDegrees;
	};
	const FFixture Fixtures[] = {
		{EOpenMobileSensorScreenRotation::Rotation0, 350.0},
		{EOpenMobileSensorScreenRotation::Rotation90, 80.0},
		{EOpenMobileSensorScreenRotation::Rotation180, 170.0},
		{EOpenMobileSensorScreenRotation::Rotation270, 260.0}
	};
	for (const FFixture& Fixture : Fixtures)
	{
		FOpenMobileSensorsScreenRotationService::ResetForTests();
		const FGuid Owner = FGuid::NewGuid();
		TestTrue(TEXT("Screen rotation fixture is captured"),
			FOpenMobileSensorsScreenRotationService::
				CaptureApplicationWindowRotation(
					Owner,
					Fixture.Rotation,
					1.0,
					false
				));
		FOpenMobileHeadingSensorSample Sample;
		Sample.Header.TimestampSeconds = 2.0;
		Sample.Header.bValid = true;
		Sample.HeadingDegrees = 350.0;
		Sample.Reference = EOpenMobileHeadingReference::MagneticNorth;
		Sample.bTiltCompensated = true;
		FOpenMobileSensorsScreenRotationService::ApplyToSample(
			Owner,
			EOpenMobileSensorCoordinateSpace::CurrentScreen,
			Sample
		);
		TestEqual(TEXT("Heading follows the selected screen top"),
			Sample.HeadingDegrees,
			Fixture.ExpectedHeadingDegrees);
		TestEqual(TEXT("Screen rotation preserves magnetic reference"),
			Sample.Reference,
			EOpenMobileHeadingReference::MagneticNorth);
		TestTrue(TEXT("Screen rotation preserves tilt compensation"),
			Sample.bTiltCompensated);
	}
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMagneticHeadingInterferenceQualityTest,
	"OpenMobile.Sensors.Heading.Magnetic.InterferenceQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMagneticHeadingInterferenceQualityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorIdentifier Sensor;
	Sensor.Type = EOpenMobileSensorType::MagneticHeading;
	Sensor.InstanceId = TEXT("Default");
	const FOpenMobileSensorAccuracySnapshot Interference =
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor,
			-1,
			1.0
		);
	TestEqual(TEXT("Magnetic interference is unreliable"),
		Interference.Accuracy,
		EOpenMobileSensorAccuracy::Unreliable);
	TestTrue(TEXT("Uncalibrated magnetic input requests calibration"),
		Interference.bCalibrationRequired);
	const FOpenMobileSensorAccuracySnapshot Recovery =
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor,
			2,
			2.0
		);
	TestEqual(TEXT("Recovered magnetic quality is high"),
		Recovery.Accuracy,
		EOpenMobileSensorAccuracy::High);
	TestFalse(TEXT("Recovered magnetic input clears calibration"),
		Recovery.bCalibrationRequired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsHeadingSmoothingTest,
	"OpenMobile.Sensors.Heading.Filtering.WrapAndDeadZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHeadingSmoothingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorFilterOptions Options;
	Options.bEnableExponentialSmoothing = true;
	Options.SmoothingTimeConstantSeconds = 1.0;
	FOpenMobileSensorHeadingFilter Filter;
	FOpenMobileHeadingSensorSample Sample;
	Sample.Header.bValid = true;
	Sample.Header.TimestampSeconds = 0.0;
	Sample.HeadingDegrees = 359.0;
	TestTrue(TEXT("The first heading initializes smoothing"),
		Filter.Apply(Options, Sample));
	TestEqual(TEXT("The first heading remains unchanged"),
		Sample.HeadingDegrees, 359.0);
	Sample.Header.TimestampSeconds = 1.0;
	Sample.HeadingDegrees = 1.0;
	TestTrue(TEXT("The wrapped heading is smoothed"),
		Filter.Apply(Options, Sample));
	TestTrue(TEXT("Smoothing follows the shortest wrapped arc"),
		FMath::IsNearlyZero(Sample.HeadingDegrees, 1.e-9));
	TestTrue(TEXT("Heading smoothing is explicit"),
		Sample.bExponentiallySmoothed);
	Options = {};
	Options.DeadZone = 2.0;
	Filter.Reset();
	Sample.Header.TimestampSeconds = 0.0;
	Sample.HeadingDegrees = 359.0;
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Heading dead zones wrap around north"),
		Sample.HeadingDegrees, 0.0);
	TestTrue(TEXT("Suppressed heading output is marked"),
		Sample.bDeadZoneSuppressed);
	Sample.Header.TimestampSeconds = 1.0;
	Sample.HeadingDegrees = 2.0;
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Heading dead-zone equality is suppressed"),
		Sample.HeadingDegrees, 0.0);
	Sample.Header.TimestampSeconds = 2.0;
	Sample.HeadingDegrees = 3.0;
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Heading outside the dead zone remains visible"),
		Sample.HeadingDegrees, 3.0);
	TestFalse(TEXT("Visible heading output is not marked suppressed"),
		Sample.bDeadZoneSuppressed);
	Options = {};
	Filter.Reset();
	Sample.Header.TimestampSeconds = 0.0;
	Sample.HeadingDegrees = 271.0;
	Filter.Apply(Options, Sample);
	TestEqual(TEXT("Disabled heading filtering is an identity"),
		Sample.HeadingDegrees, 271.0);
	TestFalse(TEXT("Disabled heading smoothing is not reported"),
		Sample.bExponentiallySmoothed);
	TestFalse(TEXT("Disabled heading dead zone is not reported"),
		Sample.bDeadZoneSuppressed);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

#include <limits>

namespace OpenMobileSensorsAbsoluteAltitudeTestsPrivate
{
	FOpenMobileSensorIdentifier MakeSensor(EOpenMobileSensorType Type)
	{
		FOpenMobileSensorIdentifier Sensor;
		Sensor.Type = Type;
		Sensor.InstanceId = TEXT("Default");
		return Sensor;
	}

	FOpenMobileSensorCapability MakeCapability(EOpenMobileSensorType Type)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeSensor(Type);
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 1.0;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor = MakeSensor(EOpenMobileSensorType::AbsoluteAltitude);
		return Request;
	}

	FOpenMobileScalarSensorSample MakeSample(
		double TimestampSeconds,
		double AltitudeMeters,
		double VerticalAccuracyMeters
	)
	{
		FOpenMobileScalarSensorSample Sample;
		Sample.Header.Sensor = MakeSensor(
			EOpenMobileSensorType::AbsoluteAltitude
		);
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.SourceFlags =
			FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
				EOpenMobileSensorType::AbsoluteAltitude
			);
		Sample.Value = AltitudeMeters;
		Sample.AbsoluteAltitude.Source =
			EOpenMobileAbsoluteAltitudeSource::NativePlatform;
		Sample.AbsoluteAltitude.bHasVerticalAccuracy = true;
		Sample.AbsoluteAltitude.VerticalAccuracyMeters =
			VerticalAccuracyMeters;
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
	FOpenMobileSensorsAbsoluteAltitudeAvailabilityTest,
	"OpenMobile.Sensors.AbsoluteAltitude.Availability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAbsoluteAltitudeAvailabilityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAbsoluteAltitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AbsoluteAltitudeUnavailable"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::BarometricPressure)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* AbsoluteAltitude =
		Snapshot.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::AbsoluteAltitude;
			}
		);
	TestNotNull(TEXT("Absolute altitude capability is explicit"),
		AbsoluteAltitude);
	if (AbsoluteAltitude)
	{
		TestEqual(TEXT("Missing native hardware is unavailable"),
			AbsoluteAltitude->Availability.State,
			EOpenMobileCapabilityState::Unavailable);
		TestFalse(TEXT("Absolute altitude has no derived fallback"),
			AbsoluteAltitude->Fallback.bAvailable);
	}
	const FOpenMobileSensorSubscriptionResult Result =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("Unavailable absolute altitude is rejected"),
		Result.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	TestEqual(TEXT("Unavailable absolute altitude reports missing hardware"),
		Result.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::MissingHardware);
	TestEqual(TEXT("No pressure-based substitute is started"),
		Backend.GetStartSensorStreamCount(), 0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAbsoluteAltitudeSamplesTest,
	"OpenMobile.Sensors.AbsoluteAltitude.SamplesAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAbsoluteAltitudeSamplesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAbsoluteAltitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AbsoluteAltitudeSamples"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::AbsoluteAltitude)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("Native absolute altitude starts"),
		Subscription.Operation.IsSuccess());
	TestEqual(TEXT("Absolute altitude uses its native stream"),
		Backend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::AbsoluteAltitude);

	FOpenMobileSensorsSampleService::PublishScalar(
		MakeSample(10.0, 120.25, 3.5)
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileScalarSensorSample Latest;
	TestTrue(TEXT("Absolute altitude sample reads"),
		FOpenMobileSensorsSampleService::ReadLatestScalar(
			Owner,
			Subscription.Handle,
			0,
			10.0,
			Read,
			Latest
		));
	TestEqual(TEXT("Altitude remains in metres"), Latest.Value, 120.25);
	TestEqual(TEXT("Core Motion timestamp is preserved"),
		Latest.Header.TimestampSeconds, 10.0);
	TestEqual(TEXT("Absolute altitude reports native source"),
		Latest.AbsoluteAltitude.Source,
		EOpenMobileAbsoluteAltitudeSource::NativePlatform);
	TestEqual(TEXT("Absolute altitude uses native fused provenance"),
		Latest.Header.SourceFlags,
		static_cast<int32>(EOpenMobileSensorSourceFlags::NativeFused));
	TestTrue(TEXT("Vertical accuracy is present"),
		Latest.AbsoluteAltitude.bHasVerticalAccuracy);
	TestEqual(TEXT("Vertical accuracy remains in metres"),
		Latest.AbsoluteAltitude.VerticalAccuracyMeters, 3.5);
	TestTrue(TEXT("Vertical accuracy populates estimated error"),
		Latest.Header.bHasEstimatedError);
	TestEqual(TEXT("Estimated error remains in metres"),
		Latest.Header.EstimatedError, 3.5);

	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSampleService::PublishScalar(
		MakeSample(11.0, 140.0, 2.0)
	);
	FOpenMobileSensorsSampleService::ReadLatestScalar(
		Owner,
		Subscription.Handle,
		0,
		11.0,
		Read,
		Latest
	);
	TestEqual(TEXT("Paused absolute altitude keeps its prior value"),
		Latest.Value, 120.25);
	TestEqual(TEXT("Paused absolute altitude read is explicit"),
		Read.Status, EOpenMobileSensorReadStatus::Paused);

	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileSensorsSampleService::PublishScalar(
		MakeSample(12.0, 123.75, -1.0)
	);
	FOpenMobileSensorsSampleService::ReadLatestScalar(
		Owner,
		Subscription.Handle,
		0,
		12.0,
		Read,
		Latest
	);
	TestEqual(TEXT("Physical elevation change is retained"),
		Latest.Value - 120.25, 3.5);
	TestFalse(TEXT("Invalid vertical accuracy becomes unavailable"),
		Latest.AbsoluteAltitude.bHasVerticalAccuracy);
	TestEqual(TEXT("Unavailable vertical accuracy is canonical"),
		Latest.AbsoluteAltitude.VerticalAccuracyMeters, 0.0);
	TestFalse(TEXT("Invalid estimated error becomes unavailable"),
		Latest.Header.bHasEstimatedError);
	TestEqual(TEXT("Unavailable estimated error is canonical"),
		Latest.Header.EstimatedError, 0.0);

	FOpenMobileScalarSensorSample InvalidAltitude =
		MakeSample(13.0, std::numeric_limits<double>::quiet_NaN(), 2.0);
	FOpenMobileSensorsSampleService::PublishScalar(InvalidAltitude);
	FOpenMobileSensorsSampleService::ReadLatestScalar(
		Owner,
		Subscription.Handle,
		0,
		13.0,
		Read,
		Latest
	);
	TestEqual(TEXT("Nonfinite altitude is ignored"), Latest.Value, 123.75);
	FOpenMobileSensorsSampleService::PublishScalar(
		MakeSample(14.0, -4.25, 1.5)
	);
	FOpenMobileSensorsSampleService::ReadLatestScalar(
		Owner,
		Subscription.Handle,
		0,
		14.0,
		Read,
		Latest
	);
	TestEqual(TEXT("Below-sea-level altitude remains signed"),
		Latest.Value, -4.25);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Subscription.Handle
	);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAbsoluteAltitudeErrorTest,
	"OpenMobile.Sensors.AbsoluteAltitude.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAbsoluteAltitudeErrorTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAbsoluteAltitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AbsoluteAltitudeError"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::AbsoluteAltitude)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorOperationResult Failure =
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable,
			TEXT("CMErrorDomain"),
			TEXT("109")
		);
	TestTrue(TEXT("Current native failure is accepted"),
		FOpenMobileSensorsSubscriptionService::FailPhysicalStreamFromBackend(
			Token,
			Backend.GetLastStartedPhysicalHandle(),
			Failure
		));
	FOpenMobileSensorSubscriptionStateSnapshot State;
	TestTrue(TEXT("Failed absolute-altitude handle remains queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Subscription.Handle,
			State
		));
	TestEqual(TEXT("Temporary native service failure queues recovery"),
		State.State,
		EOpenMobileSensorSubscriptionState::Accepted);
	TestEqual(TEXT("Temporary service reason is preserved"),
		State.Failure.Reason,
		EOpenMobileSensorFailureReason::TemporarilyUnavailable);
	TestEqual(TEXT("Native diagnostic domain is preserved"),
		State.Failure.NativeDomain,
		FString(TEXT("CMErrorDomain")));
	TestEqual(TEXT("Native diagnostic code is preserved"),
		State.Failure.NativeCode,
		FString(TEXT("109")));
	TestEqual(TEXT("Common error keeps the native code"),
		State.Error.NativeCode,
		FString(TEXT("109")));
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests(
			TNumericLimits<double>::Max()
		);
	TestTrue(TEXT("Recovered absolute-altitude handle remains queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Subscription.Handle,
			State
		));
	TestEqual(TEXT("Temporary native service failure recovers"),
		State.State,
		EOpenMobileSensorSubscriptionState::Active);
	FinishBackend(Backend);
	return true;
}

#endif

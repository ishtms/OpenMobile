#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"

#include <limits>

namespace OpenMobileSensorsCustomFrequencyTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(double FrequencyHz)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = FrequencyHz;
		Request.Options.MaximumCallbackFrequencyHz = 120.0;
		return Request;
	}

	FOpenMobileSensorCapability MakeCapability(double MaximumFrequencyHz)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("Accelerometer");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = MaximumFrequencyHz;
		return Capability;
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
	FOpenMobileSensorsFrequencyConversionPrecisionTest,
	"OpenMobile.Sensors.CustomFrequency.ConversionPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsFrequencyConversionPrecisionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	double IntervalSeconds = 0.0;
	TestTrue(TEXT("60 Hz converts to an interval"),
		UOpenMobileSensorRateLibrary::HertzToIntervalSeconds(
			60.0,
			IntervalSeconds
		));
	TestTrue(TEXT("60 Hz conversion is precise"),
		FMath::IsNearlyEqual(IntervalSeconds, 1.0 / 60.0, 1.e-12));
	double FrequencyHz = 0.0;
	TestTrue(TEXT("The interval converts back to hertz"),
		UOpenMobileSensorRateLibrary::IntervalSecondsToHertz(
			IntervalSeconds,
			FrequencyHz
		));
	TestTrue(TEXT("The round trip preserves frequency"),
		FMath::IsNearlyEqual(FrequencyHz, 60.0, 1.e-10));
	TestFalse(TEXT("Zero hertz is rejected"),
		UOpenMobileSensorRateLibrary::HertzToIntervalSeconds(
			0.0,
			IntervalSeconds
		));
	TestFalse(TEXT("Negative intervals are rejected"),
		UOpenMobileSensorRateLibrary::IntervalSecondsToHertz(
			-1.0,
			FrequencyHz
		));
	TestFalse(TEXT("Nonfinite values are rejected"),
		UOpenMobileSensorRateLibrary::HertzToIntervalSeconds(
			std::numeric_limits<double>::infinity(),
			IntervalSeconds
		));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCustomFrequencyNumericBoundariesTest,
	"OpenMobile.Sensors.CustomFrequency.NumericBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCustomFrequencyNumericBoundariesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCustomFrequencyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("FrequencyBoundaries"));
	Backend.SetSensorCapabilities({MakeCapability(1000.0)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bOriginalProjectOptIn = Settings->bAllowHighSamplingRate;
	Settings->bAllowHighSamplingRate = true;
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest Minimum = MakeRequest(1.0);
	TestEqual(TEXT("The minimum custom rate is accepted"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner, Minimum).Operation.Code,
		EOpenMobileSensorResultCode::Accepted);
	FOpenMobileSensorSubscriptionRequest Maximum = MakeRequest(1000.0);
	Maximum.Options.bAllowHighSamplingRate = true;
	TestEqual(TEXT("The opted-in maximum custom rate is accepted"),
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner, Maximum).Operation.Code,
		EOpenMobileSensorResultCode::Accepted);
	for (double InvalidFrequency : {
		0.0,
		-1.0,
		1000.0001,
		std::numeric_limits<double>::infinity(),
		std::numeric_limits<double>::quiet_NaN()
	})
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				MakeRequest(InvalidFrequency)
			);
		TestEqual(TEXT("Invalid custom rates use the frequency error"),
			Result.Operation.Failure.Reason,
			EOpenMobileSensorFailureReason::InvalidFrequency);
	}
	TestEqual(TEXT("Invalid requests never reach the backend"),
		Backend.GetStartSensorStreamCount(), 0);
	Settings->bAllowHighSamplingRate = bOriginalProjectOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCustomFrequencyRateReportingTest,
	"OpenMobile.Sensors.CustomFrequency.RateReporting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCustomFrequencyRateReportingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCustomFrequencyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RateReporting"));
	Backend.SetSensorCapabilities({MakeCapability(80.0)});
	Backend.SetAppliedStartFrequencyForTests(50.0);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(100.0)
		);
	TestEqual(TEXT("The result reports the requested rate"),
		Subscription.RateResolution.RequestedFrequencyHz, 100.0);
	TestEqual(TEXT("The result reports the capability clamp"),
		Subscription.RateResolution.ClampedFrequencyHz, 80.0);
	TestEqual(TEXT("Hardware clamp reason is explicit"),
		Subscription.RateResolution.AdjustmentReason,
		EOpenMobileSensorRateAdjustmentReason::HardwareLimit);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
	TestTrue(TEXT("The active state can be queried"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Subscription.Handle,
			Snapshot
		));
	TestEqual(TEXT("The state reports the backend-applied rate"),
		Snapshot.RateResolution.AppliedNativeFrequencyHz, 50.0);
	TestEqual(TEXT("Backend reduction reason is explicit"),
		Snapshot.RateResolution.AdjustmentReason,
		EOpenMobileSensorRateAdjustmentReason::BackendLimit);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCustomFrequencyActiveRateChangeTest,
	"OpenMobile.Sensors.CustomFrequency.ActiveRateChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCustomFrequencyActiveRateChangeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCustomFrequencyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ActiveRateChange"));
	Backend.SetSensorCapabilities({MakeCapability(120.0)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(30.0)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	Backend.SetAppliedReconfigureFrequencyForTests(45.0);
	FOpenMobileSensorStreamOptions Updated = MakeRequest(60.0).Options;
	const FOpenMobileSensorOperationResult UpdateResult =
		FOpenMobileSensorsSubscriptionService::UpdateSubscription(
			Owner,
			Subscription.Handle,
			Updated
		);
	TestTrue(TEXT("An active custom rate change succeeds"),
		UpdateResult.IsSuccess());
	FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		Owner,
		Subscription.Handle,
		Snapshot
	);
	TestEqual(TEXT("Rate changes keep the handle"),
		Snapshot.Handle, Subscription.Handle);
	TestEqual(TEXT("The state reports the changed request"),
		Snapshot.RateResolution.RequestedFrequencyHz, 60.0);
	TestEqual(TEXT("The state reports the backend reconfiguration"),
		Snapshot.RateResolution.AppliedNativeFrequencyHz, 45.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsUnsupportedCustomRateTest,
	"OpenMobile.Sensors.CustomFrequency.UnsupportedCustomRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsUnsupportedCustomRateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCustomFrequencyTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("UnsupportedCustomRate"));
	FOpenMobileSensorOperationResult Unsupported =
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	Backend.SetStartSensorStreamResult(Unsupported);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(37.0)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
	TestTrue(TEXT("A failed start remains queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Subscription.Handle,
			Snapshot
		));
	TestEqual(TEXT("Unsupported custom rates fail explicitly"),
		Snapshot.State, EOpenMobileSensorSubscriptionState::Failed);
	TestEqual(TEXT("The backend was asked exactly once"),
		Backend.GetStartSensorStreamCount(), 1);
	FinishBackend(Backend);
	return true;
}

#endif

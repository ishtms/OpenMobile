#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsPressureTestsPrivate
{
	FOpenMobileSensorIdentifier MakeSensor()
	{
		FOpenMobileSensorIdentifier Sensor;
		Sensor.Type = EOpenMobileSensorType::BarometricPressure;
		Sensor.InstanceId = TEXT("Default");
		return Sensor;
	}

	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileCapabilityState State,
		double MaximumFrequencyHz = 5.0
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeSensor();
		Capability.Availability.Name = TEXT("BarometricPressure");
		Capability.Availability.State = State;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = MaximumFrequencyHz;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor = MakeSensor();
		return Request;
	}

	FOpenMobileScalarSensorSample MakeSample(
		double TimestampSeconds,
		double PressureHectopascals
	)
	{
		FOpenMobileScalarSensorSample Sample;
		Sample.Header.Sensor = MakeSensor();
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Raw
		);
		Sample.Value = PressureHectopascals;
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
	FOpenMobileSensorsPressureUnitsAndRangeTest,
	"OpenMobile.Sensors.Pressure.UnitsAndRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPressureUnitsAndRangeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPressureTestsPrivate;
	FOpenMobileScalarSensorSample Android = MakeSample(1.0, 1013.25);
	Android.Header.bUnitsNormalized = false;
	TestTrue(TEXT("Android pressure normalizes"),
		FOpenMobileSensorUnitConverter::NormalizeScalarSample(
			EOpenMobileSensorNativePlatform::Android,
			Android
		));
	TestEqual(TEXT("Android pressure remains hectopascals"),
		Android.Value, 1013.25);
	FOpenMobileScalarSensorSample IOS = MakeSample(1.0, 101.325);
	IOS.Header.bUnitsNormalized = false;
	TestTrue(TEXT("iOS pressure normalizes"),
		FOpenMobileSensorUnitConverter::NormalizeScalarSample(
			EOpenMobileSensorNativePlatform::IOS,
			IOS
		));
	TestEqual(TEXT("iOS kilopascals become hectopascals"),
		IOS.Value, 1013.25);

	for (const double InvalidValue : {-1.0, 0.0, 2000.1})
	{
		FOpenMobileScalarSensorSample Invalid = MakeSample(2.0, InvalidValue);
		Invalid.Header.bUnitsNormalized = false;
		TestFalse(TEXT("Impossible pressure is rejected"),
			FOpenMobileSensorUnitConverter::NormalizeScalarSample(
				EOpenMobileSensorNativePlatform::Android,
				Invalid
			));
		TestFalse(TEXT("Rejected pressure is marked invalid"),
			Invalid.Header.bValid);
		TestEqual(TEXT("Invalid pressure is never replaced with sea level"),
			Invalid.Value, InvalidValue);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPressureRatePolicyTest,
	"OpenMobile.Sensors.Pressure.RatePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPressureRatePolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPressureTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PressureRate"));
	Backend.SetSensorCapabilities({MakeCapability(
		EOpenMobileCapabilityState::Available)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Default =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	TestTrue(TEXT("Default pressure request is accepted"),
		Default.Operation.IsSuccess());
	TestEqual(TEXT("Pressure defaults to one hertz"),
		Default.AppliedOptions.CustomFrequencyHz, 1.0);
	TestEqual(TEXT("Pressure callbacks default to one hertz"),
		Default.AppliedOptions.MaximumCallbackFrequencyHz, 1.0);

	FOpenMobileSensorSubscriptionRequest TooFast = MakeRequest();
	TooFast.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	TooFast.Options.CustomFrequencyHz = 6.0;
	TooFast.Options.MaximumCallbackFrequencyHz = 6.0;
	const FOpenMobileSensorSubscriptionResult Rejected =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			TooFast
		);
	TestEqual(TEXT("Unsupported pressure rate is rejected"),
		Rejected.Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPressureLifecycleAndAvailabilityTest,
	"OpenMobile.Sensors.Pressure.LifecycleAndAvailability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPressureLifecycleAndAvailabilityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPressureTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PressureLifecycle"));
	Backend.SetSensorCapabilities({MakeCapability(
		EOpenMobileCapabilityState::Available)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSampleService::PublishScalar(MakeSample(1.0, 1013.25));
	FOpenMobileSensorsSampleService::PublishScalar(MakeSample(2.0, 1013.20));
	FOpenMobileSensorReadResult Read;
	FOpenMobileScalarSensorSample Latest;
	TestTrue(TEXT("Slow pressure change is readable"),
		FOpenMobileSensorsSampleService::ReadLatestScalar(
			Owner,
			Subscription.Handle,
			0,
			2.0,
			Read,
			Latest
		));
	TestEqual(TEXT("Latest pressure preserves the measured value"),
		Latest.Value, 1013.20);
	TestEqual(TEXT("Pressure identifies a direct native source"),
		Latest.Header.SourceFlags,
		FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
			EOpenMobileSensorType::BarometricPressure
		));
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorsSampleService::PublishScalar(MakeSample(3.0, 900.0));
	FOpenMobileSensorsSampleService::ReadLatestScalar(
		Owner,
		Subscription.Handle,
		0,
		3.0,
		Read,
		Latest
	);
	TestEqual(TEXT("Paused stream retains the prior measurement"),
		Latest.Value, 1013.20);
	TestEqual(TEXT("Paused pressure read is explicit"),
		Read.Status, EOpenMobileSensorReadStatus::Paused);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileSensorsSampleService::PublishScalar(MakeSample(4.0, 1013.10));
	FOpenMobileSensorsSampleService::ReadLatestScalar(
		Owner,
		Subscription.Handle,
		0,
		4.0,
		Read,
		Latest
	);
	TestEqual(TEXT("Resumed pressure stream accepts new measurements"),
		Latest.Value, 1013.10);
	FinishBackend(Backend);

	ResetServices();
	FOpenMobileSensorsMockBackend Missing(TEXT("PressureMissing"));
	Missing.SetSensorCapabilities({MakeCapability(
		EOpenMobileCapabilityState::NotSupported)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Missing);
	const FOpenMobileSensorSubscriptionResult Unavailable =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("Unavailable pressure hardware rejects the request"),
		Unavailable.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	TestEqual(TEXT("Unavailable pressure reports missing hardware"),
		Unavailable.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::MissingHardware);
	FinishBackend(Missing);
	return true;
}

#endif

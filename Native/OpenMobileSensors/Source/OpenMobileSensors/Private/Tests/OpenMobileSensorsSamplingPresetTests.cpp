#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsSamplingPresetTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorRatePreset Preset
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = Preset;
		Request.Options.CustomFrequencyHz = 15.0;
		return Request;
	}

	FOpenMobileSensorSubscriptionResult Resolve(
		const FGuid& Owner,
		const FOpenMobileSensorSubscriptionRequest& Request
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				Request
			);
		if (Result.Handle.IsValid())
		{
			FOpenMobileSensorsSubscriptionService::StopSubscription(
				Owner,
				Result.Handle
			);
		}
		return Result;
	}

	FOpenMobileSensorCapability MakeCapability(
		double MinimumFrequencyHz,
		double MaximumFrequencyHz
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("Accelerometer");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = MinimumFrequencyHz;
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
	FOpenMobileSensorsPresetMappingTest,
	"OpenMobile.Sensors.Rates.PresetMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPresetMappingTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSamplingPresetTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PresetMapping"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const UOpenMobileSensorsSettings* Settings =
		GetDefault<UOpenMobileSensorsSettings>();
	TestEqual(TEXT("UI declares low-power intent"),
		Settings->UIPreset.PowerIntent,
		EOpenMobileSensorPowerIntent::LowPower);
	TestEqual(TEXT("Game declares balanced power intent"),
		Settings->GamePreset.PowerIntent,
		EOpenMobileSensorPowerIntent::Balanced);
	TestEqual(TEXT("Fast declares performance power intent"),
		Settings->FastPreset.PowerIntent,
		EOpenMobileSensorPowerIntent::Performance);
	const FOpenMobileSensorSubscriptionResult UI = Resolve(
		Owner, MakeRequest(EOpenMobileSensorRatePreset::UI));
	TestEqual(TEXT("UI requests 15 Hz"),
		UI.AppliedOptions.CustomFrequencyHz, 15.0);
	TestEqual(TEXT("UI allows 50 ms delivery latency"),
		UI.AppliedOptions.MaximumDeliveryLatencySeconds, 0.05);
	TestEqual(TEXT("UI callbacks are capped at 15 Hz"),
		UI.AppliedOptions.MaximumCallbackFrequencyHz, 15.0);
	const FOpenMobileSensorSubscriptionResult Game = Resolve(
		Owner, MakeRequest(EOpenMobileSensorRatePreset::Game));
	TestEqual(TEXT("Game requests 60 Hz"),
		Game.AppliedOptions.CustomFrequencyHz, 60.0);
	TestEqual(TEXT("Game allows 20 ms delivery latency"),
		Game.AppliedOptions.MaximumDeliveryLatencySeconds, 0.02);
	TestEqual(TEXT("Game callbacks are capped at 30 Hz"),
		Game.AppliedOptions.MaximumCallbackFrequencyHz, 30.0);
	const FOpenMobileSensorSubscriptionResult Fast = Resolve(
		Owner, MakeRequest(EOpenMobileSensorRatePreset::Fast));
	TestEqual(TEXT("Fast requests 200 Hz"),
		Fast.AppliedOptions.CustomFrequencyHz, 200.0);
	TestEqual(TEXT("Fast requests immediate delivery"),
		Fast.AppliedOptions.MaximumDeliveryLatencySeconds, 0.0);
	TestEqual(TEXT("Fast callbacks remain capped"),
		Fast.AppliedOptions.MaximumCallbackFrequencyHz, 60.0);
	FOpenMobileSensorSubscriptionRequest CustomRequest = MakeRequest(
		EOpenMobileSensorRatePreset::Custom);
	CustomRequest.Options.CustomFrequencyHz = 37.5;
	CustomRequest.Options.MaximumDeliveryLatencySeconds = 0.125;
	CustomRequest.Options.MaximumCallbackFrequencyHz = 12.0;
	const FOpenMobileSensorSubscriptionResult Custom = Resolve(
		Owner, CustomRequest);
	TestEqual(TEXT("Custom preserves its frequency"),
		Custom.AppliedOptions.CustomFrequencyHz, 37.5);
	TestEqual(TEXT("Custom preserves its latency"),
		Custom.AppliedOptions.MaximumDeliveryLatencySeconds, 0.125);
	TestEqual(TEXT("Custom preserves its callback cap"),
		Custom.AppliedOptions.MaximumCallbackFrequencyHz, 12.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPlatformRateClampsTest,
	"OpenMobile.Sensors.Rates.PlatformClamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPlatformRateClampsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSamplingPresetTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("PlatformClamps"));
	Backend.SetSensorCapabilities({MakeCapability(20.0, 80.0)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest High = MakeRequest(
		EOpenMobileSensorRatePreset::Custom);
	High.Options.CustomFrequencyHz = 250.0;
	High.Options.MaximumCallbackFrequencyHz = 120.0;
	const FOpenMobileSensorSubscriptionResult HighResult = Resolve(Owner, High);
	TestEqual(TEXT("Hardware maximum clamps sampling"),
		HighResult.AppliedOptions.CustomFrequencyHz, 80.0);
	TestEqual(TEXT("Callbacks cannot exceed applied sampling"),
		HighResult.AppliedOptions.MaximumCallbackFrequencyHz, 80.0);
	FOpenMobileSensorSubscriptionRequest Low = MakeRequest(
		EOpenMobileSensorRatePreset::Custom);
	Low.Options.CustomFrequencyHz = 5.0;
	const FOpenMobileSensorSubscriptionResult LowResult = Resolve(Owner, Low);
	TestEqual(TEXT("Hardware minimum raises sampling"),
		LowResult.AppliedOptions.CustomFrequencyHz, 20.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsProjectHighRatePolicyTest,
	"OpenMobile.Sensors.Rates.ProjectHighRatePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsProjectHighRatePolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSamplingPresetTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("HighRatePolicy"));
	Backend.SetSensorCapabilities({MakeCapability(1.0, 1000.0)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bOriginalProjectOptIn = Settings->bAllowHighSamplingRate;
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorRatePreset::Custom);
	Request.Options.CustomFrequencyHz = 500.0;
	Request.Options.bAllowHighSamplingRate = true;
	Settings->bAllowHighSamplingRate = false;
	const FOpenMobileSensorSubscriptionResult ProjectBlocked =
		Resolve(Owner, Request);
	TestEqual(TEXT("Project policy clamps high-rate requests"),
		ProjectBlocked.AppliedOptions.CustomFrequencyHz, 200.0);
	Settings->bAllowHighSamplingRate = true;
	Request.Options.bAllowHighSamplingRate = false;
	const FOpenMobileSensorSubscriptionResult RequestBlocked =
		Resolve(Owner, Request);
	TestEqual(TEXT("The request also needs an explicit high-rate opt-in"),
		RequestBlocked.AppliedOptions.CustomFrequencyHz, 200.0);
	Request.Options.bAllowHighSamplingRate = true;
	const FOpenMobileSensorSubscriptionResult Allowed = Resolve(Owner, Request);
	TestEqual(TEXT("Both opt-ins allow the hardware-supported rate"),
		Allowed.AppliedOptions.CustomFrequencyHz, 500.0);
	Settings->bAllowHighSamplingRate = bOriginalProjectOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsInvalidRatePresetTest,
	"OpenMobile.Sensors.Rates.InvalidPreset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsInvalidRatePresetTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSamplingPresetTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("InvalidPreset"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorRatePreset::UI);
	Request.Options.RatePreset = static_cast<EOpenMobileSensorRatePreset>(255);
	const FOpenMobileSensorSubscriptionResult Result = Resolve(
		FGuid::NewGuid(), Request);
	TestEqual(TEXT("Unknown preset values are rejected"),
		Result.Operation.Code, EOpenMobileSensorResultCode::InvalidArgument);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRateSettingsOverrideTest,
	"OpenMobile.Sensors.Rates.SettingsOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRateSettingsOverrideTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSamplingPresetTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("SettingsOverride"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const FOpenMobileSensorRatePresetSettings OriginalUI = Settings->UIPreset;
	Settings->UIPreset.RequestedFrequencyHz = 24.0;
	Settings->UIPreset.MaximumDeliveryLatencySeconds = 0.1;
	Settings->UIPreset.MaximumCallbackFrequencyHz = 12.0;
	const FOpenMobileSensorSubscriptionResult Result = Resolve(
		FGuid::NewGuid(), MakeRequest(EOpenMobileSensorRatePreset::UI));
	TestEqual(TEXT("Runtime settings override preset frequency"),
		Result.AppliedOptions.CustomFrequencyHz, 24.0);
	TestEqual(TEXT("Runtime settings override preset latency"),
		Result.AppliedOptions.MaximumDeliveryLatencySeconds, 0.1);
	TestEqual(TEXT("Runtime settings override callback cap"),
		Result.AppliedOptions.MaximumCallbackFrequencyHz, 12.0);
	Settings->UIPreset = OriginalUI;
	FinishBackend(Backend);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsHighSamplingTestsPrivate
{
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

	FOpenMobileSensorSubscriptionRequest MakeRequest(double FrequencyHz)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = FrequencyHz;
		Request.Options.MaximumCallbackFrequencyHz = 120.0;
		Request.Options.bAllowHighSamplingRate = true;
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
	FOpenMobileSensorsHighSamplingManifestOffTest,
	"OpenMobile.Sensors.HighSampling.ManifestOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHighSamplingManifestOffTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsHighSamplingTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bOriginalOptIn = Settings->bAllowHighSamplingRate;
	Settings->bAllowHighSamplingRate = true;
	FOpenMobileSensorsMockBackend Backend(TEXT("ManifestOff"));
	Backend.SetSensorCapabilities({MakeCapability(400.0)});
	Backend.SetHighSamplingRateDeclarationForTests(true, false);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorSubscriptionResult Result =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest(300.0)
		);
	TestEqual(TEXT("A missing declaration clamps to the normal ceiling"),
		Result.AppliedOptions.CustomFrequencyHz, 200.0);
	TestEqual(TEXT("The missing declaration has a structured reason"),
		Result.RateResolution.AdjustmentReason,
		EOpenMobileSensorRateAdjustmentReason::MissingPlatformDeclaration);
	Settings->bAllowHighSamplingRate = bOriginalOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsHighSamplingManifestOnTest,
	"OpenMobile.Sensors.HighSampling.ManifestOn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHighSamplingManifestOnTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsHighSamplingTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bOriginalOptIn = Settings->bAllowHighSamplingRate;
	Settings->bAllowHighSamplingRate = true;
	FOpenMobileSensorsMockBackend Backend(TEXT("ManifestOn"));
	Backend.SetSensorCapabilities({MakeCapability(400.0)});
	Backend.SetHighSamplingRateDeclarationForTests(true, true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Result =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(300.0)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("The declared high rate reaches the backend"),
		Backend.GetLastStartedPhysicalRequest().RequestedFrequencyHz, 300.0);
	TestEqual(TEXT("The declared high rate remains applied"),
		Result.AppliedOptions.CustomFrequencyHz, 300.0);
	Settings->bAllowHighSamplingRate = bOriginalOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsHighSamplingBoundaryRatesAndPresetsTest,
	"OpenMobile.Sensors.HighSampling.BoundaryRatesAndPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHighSamplingBoundaryRatesAndPresetsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsHighSamplingTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bOriginalOptIn = Settings->bAllowHighSamplingRate;
	Settings->bAllowHighSamplingRate = false;
	FOpenMobileSensorsMockBackend Backend(TEXT("HighRateBoundary"));
	Backend.SetSensorCapabilities({MakeCapability(400.0)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorSubscriptionRequest Boundary = MakeRequest(200.0);
	Boundary.Options.bAllowHighSamplingRate = false;
	const FOpenMobileSensorSubscriptionResult AtBoundary =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(), Boundary);
	const FOpenMobileSensorSubscriptionResult AboveBoundary =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(), MakeRequest(200.01));
	TestEqual(TEXT("The normal 200 Hz boundary needs no opt-in"),
		AtBoundary.AppliedOptions.CustomFrequencyHz, 200.0);
	TestEqual(TEXT("A rate above the boundary clamps safely"),
		AboveBoundary.AppliedOptions.CustomFrequencyHz, 200.0);
	TestEqual(TEXT("The project clamp has a structured reason"),
		AboveBoundary.RateResolution.AdjustmentReason,
		EOpenMobileSensorRateAdjustmentReason::ProjectPolicy);
	FOpenMobileSensorSubscriptionRequest UI = MakeRequest(300.0);
	UI.Options.RatePreset = EOpenMobileSensorRatePreset::UI;
	UI.Options.bAllowHighSamplingRate = false;
	const FOpenMobileSensorSubscriptionResult UIResult =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(), UI);
	TestEqual(TEXT("The ordinary UI preset remains available"),
		UIResult.AppliedOptions.CustomFrequencyHz,
		Settings->UIPreset.RequestedFrequencyHz);
	Settings->bAllowHighSamplingRate = bOriginalOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsHighSamplingRuntimeClampTest,
	"OpenMobile.Sensors.HighSampling.RuntimeClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHighSamplingRuntimeClampTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsHighSamplingTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bOriginalOptIn = Settings->bAllowHighSamplingRate;
	Settings->bAllowHighSamplingRate = true;
	FOpenMobileSensorsMockBackend Backend(TEXT("RuntimeClamp"));
	Backend.SetSensorCapabilities({MakeCapability(400.0)});
	Backend.SetHighSamplingRateDeclarationForTests(true, true);
	Backend.SetAppliedStartFrequencyForTests(200.0);
	Backend.SetAppliedRateAdjustmentReasonForTests(
		EOpenMobileSensorRateAdjustmentReason::OperatingSystemLimit
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Result =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(300.0)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorSubscriptionStateSnapshot State;
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		Owner, Result.Handle, State);
	TestEqual(TEXT("The OS-applied rate is reported"),
		State.RateResolution.AppliedNativeFrequencyHz, 200.0);
	TestEqual(TEXT("The runtime OS clamp has a structured reason"),
		State.RateResolution.AdjustmentReason,
		EOpenMobileSensorRateAdjustmentReason::OperatingSystemLimit);
	Settings->bAllowHighSamplingRate = bOriginalOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsHighSamplingDeviceRateBoundariesTest,
	"OpenMobile.Sensors.HighSampling.DeviceRateBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHighSamplingDeviceRateBoundariesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsHighSamplingTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bOriginalOptIn = Settings->bAllowHighSamplingRate;
	Settings->bAllowHighSamplingRate = true;
	FOpenMobileSensorsMockBackend Backend(TEXT("DeviceRateBoundaries"));
	Backend.SetSensorCapabilities({MakeCapability(150.0)});
	Backend.SetHighSamplingRateDeclarationForTests(true, true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorSubscriptionResult BelowRestrictedDevice =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(), MakeRequest(300.0));
	TestEqual(TEXT("A lower-rate device clamps to hardware"),
		BelowRestrictedDevice.AppliedOptions.CustomFrequencyHz, 150.0);
	TestEqual(TEXT("The hardware clamp remains distinct"),
		BelowRestrictedDevice.RateResolution.AdjustmentReason,
		EOpenMobileSensorRateAdjustmentReason::HardwareLimit);
	Settings->bAllowHighSamplingRate = bOriginalOptIn;
	FinishBackend(Backend);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileDeviceBlueprintExamples.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceSamplePowerPolicyTest,
	"OpenMobile.Device.Sample.PowerPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceSamplePowerPolicyTest::RunTest(const FString& Parameters)
{
	FOpenMobilePowerSnapshot Power;
	Power.bPowerSavingEnabled = FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	TestTrue(
		TEXT("Power saving lowers quality"),
		UOpenMobileDeviceBlueprintExamples::ShouldReduceQualityForPower(Power)
	);

	Power.bPowerSavingEnabled = FOpenMobileDeviceOptionalBool::MakeAvailable(false);
	Power.ThermalState = EOpenMobileThermalState::Critical;
	TestTrue(
		TEXT("Critical thermal state lowers quality"),
		UOpenMobileDeviceBlueprintExamples::ShouldReduceQualityForPower(Power)
	);

	Power.ThermalState = EOpenMobileThermalState::Nominal;
	TestFalse(
		TEXT("Nominal power keeps quality"),
		UOpenMobileDeviceBlueprintExamples::ShouldReduceQualityForPower(Power)
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceSampleDownloadPolicyTest,
	"OpenMobile.Device.Sample.DownloadPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceSampleDownloadPolicyTest::RunTest(const FString& Parameters)
{
	FOpenMobileStorageSnapshot Storage;
	Storage.AvailableBytes = FOpenMobileDeviceOptionalInt64::MakeAvailable(1024);
	Storage.bIsLowStorage = FOpenMobileDeviceOptionalBool::MakeAvailable(false);

	FOpenMobileNetworkPathSnapshot Network;
	Network.PathState = EOpenMobileNetworkPathState::InternetCapable;
	TestTrue(
		TEXT("Unmetered path with space allows download"),
		UOpenMobileDeviceBlueprintExamples::ShouldAllowDownload(
			Storage,
			Network,
			512
		)
	);

	Network.bIsMetered = FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	TestFalse(
		TEXT("Metered path blocks policy download"),
		UOpenMobileDeviceBlueprintExamples::ShouldAllowDownload(
			Storage,
			Network,
			512
		)
	);

	Network.bIsMetered = FOpenMobileDeviceOptionalBool::MakeAvailable(false);
	Storage.bIsLowStorage = FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	TestFalse(
		TEXT("Low storage blocks policy download"),
		UOpenMobileDeviceBlueprintExamples::ShouldAllowDownload(
			Storage,
			Network,
			512
		)
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceSampleTransitionPolicyTest,
	"OpenMobile.Device.Sample.TransitionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceSampleTransitionPolicyTest::RunTest(const FString& Parameters)
{
	FOpenMobileNetworkPathSnapshot PreviousNetwork;
	PreviousNetwork.PathState = EOpenMobileNetworkPathState::InternetCapable;
	PreviousNetwork.bDefaultTransportAvailable = true;
	PreviousNetwork.DefaultTransport = EOpenMobileNetworkTransport::Wifi;
	FOpenMobileNetworkPathSnapshot CurrentNetwork = PreviousNetwork;
	CurrentNetwork.DefaultTransport = EOpenMobileNetworkTransport::Cellular;
	TestTrue(
		TEXT("Transport change is a handoff"),
		UOpenMobileDeviceBlueprintExamples::IsNetworkHandoff(
			PreviousNetwork,
			CurrentNetwork
		)
	);

	FOpenMobileWindowDisplaySnapshot PreviousWindow;
	FOpenMobileWindowDisplaySnapshot CurrentWindow;
	CurrentWindow.SafeAreaInsets.bIsAvailable = true;
	CurrentWindow.SafeAreaInsets.Top = 48.0f;
	TestTrue(
		TEXT("Inset change rebuilds safe layout"),
		UOpenMobileDeviceBlueprintExamples::ShouldRebuildSafeArea(
			PreviousWindow,
			CurrentWindow
		)
	);

	FOpenMobileDeviceCapability Capability;
	Capability.State = EOpenMobileCapabilityState::Denied;
	TestTrue(
		TEXT("Denied capability offers settings recovery"),
		UOpenMobileDeviceBlueprintExamples::ShouldRecoverThroughSettings(Capability)
	);
	Capability.State = EOpenMobileCapabilityState::NotSupported;
	TestFalse(
		TEXT("Unsupported capability does not offer settings recovery"),
		UOpenMobileDeviceBlueprintExamples::ShouldRecoverThroughSettings(Capability)
	);
	return true;
}

#endif

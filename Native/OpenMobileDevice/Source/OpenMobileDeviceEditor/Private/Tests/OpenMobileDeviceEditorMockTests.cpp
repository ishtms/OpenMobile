#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "IOpenMobileDeviceBackend.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBlueprintLibrary.h"
#include "OpenMobileDeviceEditorMock.h"
#include "OpenMobileDeviceMockSettings.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceEditorMockSettingsTest,
	"OpenMobile.Device.EditorMock.SettingsAndSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceEditorMockSettingsTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileDeviceEditorMock::ResetForTests();

	const UOpenMobileDeviceMockSettings* Settings =
		GetDefault<UOpenMobileDeviceMockSettings>();
	TestEqual(TEXT("Settings category"), Settings->GetCategoryName(), FName(TEXT("OpenMobile")));
	TestEqual(
		TEXT("Settings section"),
		Settings->GetSectionName(),
		FName(TEXT("OpenMobile Device Mock"))
	);
	TestEqual(
		TEXT("Settings display name"),
		Settings->GetClass()->GetMetaData(TEXT("DisplayName")),
		FString(TEXT("OpenMobile Device Mock"))
	);
	TestEqual(
		TEXT("Settings stay in editor user config"),
		Settings->GetClass()->ClassConfigName,
		FName(TEXT("EditorPerProjectUserSettings"))
	);
	TestFalse(TEXT("Mock defaults to disabled"), Settings->bEnableMockBackend);
	TestFalse(TEXT("Mock starts unregistered"), FOpenMobileDeviceEditorMock::IsEnabled());

	FOpenMobileDeviceEditorMock::SetEnabled(true);
	TestTrue(TEXT("Explicit enable registers the mock"), FOpenMobileDeviceEditorMock::IsEnabled());
	TestTrue(TEXT("Explicitly enabled mock is selected"), FOpenMobileDeviceEditorMock::IsSelected());
	TestEqual(
		TEXT("Mock backend has a stable name"),
		FOpenMobileDeviceBackendRegistry::FindBackend()->GetBackendName(),
		FName(TEXT("OpenMobileDeviceEditorMock"))
	);

	FOpenMobileDeviceEditorMock::SetEnabled(false);
	TestFalse(TEXT("Disable unregisters the mock"), FOpenMobileDeviceEditorMock::IsEnabled());
	TestFalse(TEXT("Disabled mock is not selected"), FOpenMobileDeviceEditorMock::IsSelected());
	FOpenMobileDeviceEditorMock::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceEditorMockStateTest,
	"OpenMobile.Device.EditorMock.StateAndFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceEditorMockStateTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileDeviceEditorMock::ResetForTests();
	FOpenMobileDeviceEditorMock::SetEnabled(true);

	FOpenMobileDeviceMockState State;
	State.Power.BatteryPercent = FOpenMobileDeviceOptionalFloat::MakeAvailable(8.0f);
	State.Power.ChargingState = EOpenMobileBatteryChargingState::Charging;
	State.Power.ChargingSource = EOpenMobileChargingSource::USB;
	State.Power.ThermalState = EOpenMobileThermalState::Critical;
	State.Memory.AvailablePhysicalBytes =
		FOpenMobileDeviceOptionalInt64::MakeAvailable(64ll * 1024 * 1024);
	State.Memory.PressureState = EOpenMobileMemoryPressureState::Critical;
	State.Storage.AvailableBytes =
		FOpenMobileDeviceOptionalInt64::MakeAvailable(128ll * 1024 * 1024);
	State.Storage.bIsLowStorage = FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	State.Network.PathState = EOpenMobileNetworkPathState::InternetCapable;
	State.Network.bDefaultTransportAvailable = true;
	State.Network.DefaultTransport = EOpenMobileNetworkTransport::Wifi;
	State.Window.bDisplayCutoutsAvailable = true;
	State.Window.DisplayCutouts.Add({0.0f, 0.0f, 120.0f, 36.0f});
	State.Window.FoldablePosture = EOpenMobileFoldablePosture::Tabletop;
	State.Appearance.Appearance = EOpenMobileSystemAppearance::Dark;
	State.Accessibility.PreferredTextScale =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(1.4f);
	State.Accessibility.bReducedAnimationPreferred =
		FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	State.Accessibility.bScreenReaderActive =
		FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	State.Accessibility.bTouchExplorationActive =
		FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	State.Failures.Add(
		FOpenMobileDeviceCapabilityNames::StorageSpace,
		FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("scripted storage failure")
		)
	);
	FOpenMobileDeviceEditorMock::SetState(State);
	IOpenMobileDeviceBackend* Backend = FOpenMobileDeviceBackendRegistry::FindBackend();

	const FOpenMobilePowerSnapshot Power = Backend->GetPowerSnapshot();
	TestEqual(TEXT("Low battery is scripted"), Power.BatteryPercent.Value, 8.0f);
	TestEqual(TEXT("Charging is scripted"), Power.ChargingState, EOpenMobileBatteryChargingState::Charging);
	TestEqual(TEXT("Thermal pressure is scripted"), Power.ThermalState, EOpenMobileThermalState::Critical);
	TestEqual(
		TEXT("Low memory is scripted"),
		Backend->GetMemorySnapshot().PressureState,
		EOpenMobileMemoryPressureState::Critical
	);
	TestTrue(
		TEXT("Low storage is scripted"),
		Backend->GetStorageSnapshot().bIsLowStorage.Value
	);
	TestEqual(
		TEXT("Network path is scripted"),
		Backend->GetNetworkPathSnapshot().DefaultTransport,
		EOpenMobileNetworkTransport::Wifi
	);
	const FOpenMobileWindowDisplaySnapshot Window =
		Backend->GetWindowDisplaySnapshot();
	TestEqual(TEXT("Display cutout is scripted"), Window.DisplayCutouts.Num(), 1);
	TestEqual(TEXT("Fold posture is scripted"), Window.FoldablePosture, EOpenMobileFoldablePosture::Tabletop);
	TestEqual(
		TEXT("Appearance is scripted"),
		Backend->GetAppearanceSnapshot().Appearance,
		EOpenMobileSystemAppearance::Dark
	);
	const FOpenMobileAccessibilitySnapshot Accessibility =
		Backend->GetAccessibilitySnapshot();
	TestEqual(TEXT("Text scale is scripted"), Accessibility.PreferredTextScale.Value, 1.4f);
	TestTrue(TEXT("Reduced animation is scripted"), Accessibility.bReducedAnimationPreferred.Value);
	TestTrue(TEXT("Assistive state is scripted"), Accessibility.bScreenReaderActive.Value);

	FOpenMobileStorageSnapshot Storage;
	FOpenMobileError Error;
	TestFalse(TEXT("Storage query failure is injected"), Backend->QueryStorageSnapshot(Storage, Error));
	TestEqual(TEXT("Scripted failure code is preserved"), Error.Code, EOpenMobileErrorCode::NativeFailure);
	TestEqual(
		TEXT("Failed capability is temporarily unavailable"),
		Backend->GetCapability(FOpenMobileDeviceCapabilityNames::StorageSpace).State,
		EOpenMobileCapabilityState::TemporarilyUnavailable
	);

	FOpenMobileDeviceEditorMock::ResetForTests();
	TestFalse(TEXT("Test reset disables the backend"), FOpenMobileDeviceEditorMock::IsEnabled());
	TestEqual(TEXT("Test reset clears failures"), FOpenMobileDeviceEditorMock::GetState().Failures.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceEditorMockEventsTest,
	"OpenMobile.Device.EditorMock.ScriptedEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceEditorMockEventsTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceEditorMock::ResetForTests();
	FOpenMobileDeviceEditorMock::SetEnabled(true);

	TArray<float> ObservedBatteryLevels;
	bool bAllEventsOnGameThread = true;
	const FDelegateHandle EventHandle =
		FOpenMobileDeviceMonitoringService::OnGroupChanged().AddLambda(
			[&ObservedBatteryLevels, &bAllEventsOnGameThread](
				EOpenMobileDeviceMonitoringGroup Group
			)
			{
				if (Group != EOpenMobileDeviceMonitoringGroup::Power)
				{
					return;
				}
				bAllEventsOnGameThread &= IsInGameThread();
				ObservedBatteryLevels.Add(
					FOpenMobileDeviceBackendRegistry::FindBackend()
						->GetPowerSnapshot().BatteryPercent.Value
				);
			}
		);
	const FGuid Subscription = FOpenMobileDeviceMonitoringService::AddSubscription(
		{EOpenMobileDeviceMonitoringGroup::Power},
		1.0f
	);

	auto MakeStep = [](float BatteryPercent, EOpenMobileDeviceMockEventDelivery Delivery)
	{
		FOpenMobileDeviceMockScriptStep Step;
		Step.Group = EOpenMobileDeviceMonitoringGroup::Power;
		Step.Delivery = Delivery;
		Step.State.Power.BatteryPercent =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(BatteryPercent);
		return Step;
	};
	FOpenMobileDeviceEditorMock::QueueScriptStep(
		MakeStep(10.0f, EOpenMobileDeviceMockEventDelivery::Duplicate)
	);
	FOpenMobileDeviceEditorMock::QueueScriptStep(
		MakeStep(20.0f, EOpenMobileDeviceMockEventDelivery::OutOfOrder)
	);
	FOpenMobileDeviceEditorMock::QueueScriptStep(
		MakeStep(30.0f, EOpenMobileDeviceMockEventDelivery::Stale)
	);
	FOpenMobileDeviceEditorMock::QueueScriptStep(
		MakeStep(40.0f, EOpenMobileDeviceMockEventDelivery::OffThread)
	);
	FOpenMobileDeviceMockScriptStep Delayed =
		MakeStep(50.0f, EOpenMobileDeviceMockEventDelivery::Delayed);
	Delayed.DelaySeconds = 0.5f;
	FOpenMobileDeviceEditorMock::QueueScriptStep(Delayed);
	TestEqual(TEXT("Script queue is visible"), FOpenMobileDeviceEditorMock::GetQueuedScriptStepCount(), 5);

	TestTrue(TEXT("Duplicate step runs"), FOpenMobileDeviceEditorMock::RunNextScriptStep());
	TestTrue(TEXT("Out-of-order step runs"), FOpenMobileDeviceEditorMock::RunNextScriptStep());
	TestTrue(TEXT("Stale step runs"), FOpenMobileDeviceEditorMock::RunNextScriptStep());
	TestEqual(TEXT("Duplicate and stale callbacks are rejected"), ObservedBatteryLevels.Num(), 2);
	TestEqual(TEXT("First sequence is stable"), ObservedBatteryLevels[0], 10.0f);
	TestEqual(TEXT("Second sequence is stable"), ObservedBatteryLevels[1], 20.0f);

	TestTrue(TEXT("Off-thread step runs"), FOpenMobileDeviceEditorMock::RunNextScriptStep());
	FOpenMobileDeviceEditorMock::FlushBackgroundEventsForTests();
	TestEqual(TEXT("Off-thread event is delivered once"), ObservedBatteryLevels.Num(), 3);
	TestEqual(TEXT("Off-thread state is visible"), ObservedBatteryLevels[2], 40.0f);

	TestTrue(TEXT("Delayed step runs"), FOpenMobileDeviceEditorMock::RunNextScriptStep());
	FOpenMobileDeviceEditorMock::TickForTests(0.49f);
	TestEqual(TEXT("Delayed event waits"), ObservedBatteryLevels.Num(), 3);
	FOpenMobileDeviceEditorMock::TickForTests(0.01f);
	TestEqual(TEXT("Delayed event is delivered once"), ObservedBatteryLevels.Num(), 4);
	TestEqual(TEXT("Delayed state is visible"), ObservedBatteryLevels[3], 50.0f);
	TestTrue(TEXT("Callbacks settle on the game thread"), bAllEventsOnGameThread);
	TestEqual(TEXT("Script queue drains deterministically"), FOpenMobileDeviceEditorMock::GetQueuedScriptStepCount(), 0);

	FOpenMobileDeviceEditorMock::QueueScriptStep(
		MakeStep(60.0f, EOpenMobileDeviceMockEventDelivery::Delayed)
	);
	FOpenMobileDeviceEditorMock::ResetOverrides();
	TestEqual(TEXT("Reset clears pending script"), FOpenMobileDeviceEditorMock::GetQueuedScriptStepCount(), 0);
	FOpenMobileDeviceEditorMock::TickForTests(1.0f);
	TestEqual(TEXT("Reset cancels delayed delivery"), ObservedBatteryLevels.Num(), 4);

	FOpenMobileDeviceMonitoringService::RemoveSubscription(Subscription);
	FOpenMobileDeviceMonitoringService::OnGroupChanged().Remove(EventHandle);
	FOpenMobileDeviceEditorMock::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceEditorMockPublicContractsTest,
	"OpenMobile.Device.EditorMock.PublicContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceEditorMockPublicContractsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using Group = EOpenMobileDeviceMonitoringGroup;
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceEditorMock::ResetForTests();
	FOpenMobileDeviceEditorMock::SetEnabled(true);

	FOpenMobileDeviceMockState State;
	State.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(42.0f);
	FOpenMobileDeviceEditorMock::SetState(State);
	TestEqual(
		TEXT("Blueprint battery node reads the selected mock"),
		UOpenMobileDeviceBlueprintLibrary::GetBatteryPercent(),
		int32(42)
	);
	TestEqual(
		TEXT("Blueprint capability node reports mock support"),
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			FOpenMobileDeviceCapabilityNames::BatteryLevel
		).State,
		EOpenMobileCapabilityState::Available
	);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	TestEqual(
		TEXT("Public C++ snapshot reads the same mock state"),
		Subsystem->GetPowerSnapshot().BatteryPercent.Value,
		42.0f
	);
	int32 PublicEventCount = 0;
	Subsystem->OnNativePowerSnapshotChanged().AddLambda(
		[&PublicEventCount](const FOpenMobilePowerSnapshot& Snapshot)
		{
			if (Snapshot.BatteryPercent.bIsAvailable
				&& Snapshot.BatteryPercent.Value == 21.0f)
			{
				++PublicEventCount;
			}
		}
	);
	UOpenMobileDeviceMonitoringSubscription* Subscription =
		Subsystem->StartMonitoring(GameInstance, {Group::Power}, 1.0f);
	TestNotNull(TEXT("Blueprint monitoring contract returns a subscription"), Subscription);

	FOpenMobileDeviceMockScriptStep Step;
	Step.Group = Group::Power;
	Step.State.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(21.0f);
	FOpenMobileDeviceEditorMock::QueueScriptStep(Step);
	TestTrue(TEXT("Scripted public event runs"), FOpenMobileDeviceEditorMock::RunNextScriptStep());
	TestEqual(TEXT("Public snapshot delegate receives the scripted state"), PublicEventCount, 1);

	Subscription->Stop();
	Subsystem->Deinitialize();
	FOpenMobileDeviceEditorMock::SetEnabled(false);
	TestEqual(
		TEXT("Blueprint battery node returns its unsupported sentinel"),
		UOpenMobileDeviceBlueprintLibrary::GetBatteryPercent(),
		int32(-1)
	);
	TestEqual(
		TEXT("Blueprint capability node preserves unsupported state"),
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			FOpenMobileDeviceCapabilityNames::BatteryLevel
		).State,
		EOpenMobileCapabilityState::NotSupported
	);
	FOpenMobileDeviceEditorMock::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	return true;
}

#endif

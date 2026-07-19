#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsEditorMockBackend.h"
#include "OpenMobileSensorsEditorMockSettings.h"
#include "OpenMobileSensorsSettings.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsEditorMockTest,
	"OpenMobile.Sensors.EditorMock.Controls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsEditorMockTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	UOpenMobileSensorsEditorMockSettings* MockSettings =
		NewObject<UOpenMobileSensorsEditorMockSettings>();
	TestEqual(TEXT("Mock settings use the OpenMobile category"),
		MockSettings->GetCategoryName(), FName(TEXT("OpenMobile")));
	TestEqual(TEXT("Mock settings have their own section"),
		MockSettings->GetSectionName(),
		FName(TEXT("OpenMobile Sensors Mocks")));
#if WITH_METADATA
	TestEqual(TEXT("Mock settings display name matches its section"),
		MockSettings->GetClass()->GetMetaData(TEXT("DisplayName")),
		FString(TEXT("OpenMobile Sensors Mocks")));
#endif

	FOpenMobileSensorsEditorMockBackend Backend;
	Backend.Activate();
	const bool bRegistered =
		FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	TestTrue(TEXT("Mock backend registers"), bRegistered);
	if (bRegistered)
	{
		TestEqual(TEXT("Mock backend wins selection"),
			FOpenMobileSensorsBackendRegistry::FindBackend(),
			static_cast<IOpenMobileSensorsBackend*>(&Backend));
	}

	FOpenMobileSensorsMockInput Input;
	Input.HeadingDegrees = 275.0;
	Input.StepCount = 42;
	Input.Activity = EOpenMobileMotionActivity::Walking;
	Input.PressureHectopascals = 1007.5;
	Input.AmbientLightLux = 125.0;
	Input.bProximityNear = true;
	TestTrue(TEXT("Explicit controls accept valid values"),
		Backend.ApplyInput(Input).IsSuccess());

	Input.HeadingDegrees = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Unsupported control values are rejected"),
		Backend.ApplyInput(Input).IsSuccess());
	TestTrue(TEXT("Deterministic presets apply"),
		Backend.ApplyPreset(
			EOpenMobileSensorsMockPreset::Running
		).IsSuccess());

	FOpenMobileSensorsMockTimeline Timeline;
	Timeline.bUseManualClock = true;
	FOpenMobileSensorsMockTimelineFrame& First =
		Timeline.Frames.AddDefaulted_GetRef();
	First.Input.MotionActivityPermission =
		EOpenMobilePermissionStatus::Granted;
	FOpenMobileSensorsMockTimelineFrame& Second =
		Timeline.Frames.AddDefaulted_GetRef();
	Second.TimeSeconds = 1.0;
	Second.Input.MotionActivityPermission =
		EOpenMobilePermissionStatus::Denied;
	TestTrue(TEXT("Scripted timeline starts"),
		Backend.PlayTimeline(Timeline).IsSuccess());
	const FName MotionPermission =
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity
		);
	TestEqual(TEXT("Timeline begins at its first frame"),
		Backend.GetStatus(MotionPermission).Status,
		EOpenMobilePermissionStatus::Granted);
	TestTrue(TEXT("Manual timeline advances deterministically"),
		Backend.AdvanceTimeline(1.0).IsSuccess());
	TestEqual(TEXT("Timeline applies its next frame"),
		Backend.GetStatus(MotionPermission).Status,
		EOpenMobilePermissionStatus::Denied);

	Backend.BeginShutdown();
	TestFalse(TEXT("PIE shutdown disables mock input"), Backend.IsActive());
	Backend.Activate();
	TestTrue(TEXT("PIE restart restores mock controls"),
		Backend.ApplyPreset(
			EOpenMobileSensorsMockPreset::Stationary
		).IsSuccess());

	UOpenMobileSensorsSettings* ShippingSettings =
		NewObject<UOpenMobileSensorsSettings>();
	ShippingSettings->DevelopmentInputMode =
		EOpenMobileSensorsDevelopmentInputMode::Mock;
	TArray<FString> Errors;
	TestFalse(TEXT("Shipping excludes mock selection"),
		ShippingSettings->Validate(Errors, true));

	if (bRegistered)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	}
	return true;
}

#endif

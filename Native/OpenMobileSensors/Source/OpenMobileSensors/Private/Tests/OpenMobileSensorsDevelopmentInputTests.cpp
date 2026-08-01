#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsDevelopmentInput.h"
#include "OpenMobileSensorsSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDevelopmentBlueprintContractTest,
	"OpenMobile.Sensors.Blueprint.Development.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDevelopmentBlueprintContractTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const EOpenMobileSensorsDevelopmentInputMode SavedMode =
		Settings->DevelopmentInputMode;
	Settings->DevelopmentInputMode =
		EOpenMobileSensorsDevelopmentInputMode::Disabled;
	EOpenMobileSensorMockActionOutcome Outcome =
		EOpenMobileSensorMockActionOutcome::Failed;
	FText Message;
	FText Correction;
	FOpenMobileSensorOperationResult Details;
	UOpenMobileSensorsDevelopmentLibrary::ApplyMockPresetWithOutcome(
		EOpenMobileSensorsMockPreset::Walking,
		Outcome,
		Message,
		Correction,
		Details
	);
	TestEqual(TEXT("Disabled mocks use a dedicated branch"),
		Outcome, EOpenMobileSensorMockActionOutcome::MocksInactive);
	TestTrue(TEXT("Inactive mocks have a clear message"),
		Message.ToString().Contains(TEXT("Mocks are inactive")));
	TestTrue(TEXT("The correction names the exact settings section"),
		Correction.ToString().Contains(
			TEXT("Project Settings > OpenMobile > OpenMobile Sensors")));
	TestEqual(TEXT("Inactive details remain machine-readable"),
		Details.Failure.Reason,
		EOpenMobileSensorFailureReason::ConfigurationBlocked);
	TestFalse(TEXT("The pure status agrees with the action branch"),
		UOpenMobileSensorsDevelopmentLibrary::IsMockInputActive());

#if WITH_METADATA
	for (const FName FunctionName : {
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorsDevelopmentLibrary,
			ApplyMockInputWithOutcome),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorsDevelopmentLibrary,
			ApplyMockPresetWithOutcome),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorsDevelopmentLibrary,
			PlayMockTimelineWithOutcome),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorsDevelopmentLibrary,
			StopMockTimelineWithOutcome),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorsDevelopmentLibrary,
			AdvanceMockTimelineWithOutcome),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorsDevelopmentLibrary,
			InjectMockErrorWithOutcome)})
	{
		const UFunction* Function =
			UOpenMobileSensorsDevelopmentLibrary::StaticClass()->
				FindFunctionByName(FunctionName);
		TestNotNull(TEXT("The preferred development action is reflected"),
			Function);
		if (Function)
		{
			TestEqual(TEXT("Development outcomes become execution pins"),
				Function->GetMetaData(TEXT("ExpandEnumAsExecs")),
				FString(TEXT("Outcome")));
			TestTrue(TEXT("The node is marked development-only"),
				Function->HasMetaData(TEXT("DevelopmentOnly")));
			TestTrue(TEXT("The tooltip explains non-Shipping behavior"),
				Function->GetToolTipText().ToString().Contains(
					TEXT("non-Shipping")));
		}
	}
	const UFunction* StatusFunction =
		UOpenMobileSensorsDevelopmentLibrary::StaticClass()->
			FindFunctionByName(GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorsDevelopmentLibrary,
				IsMockInputActive));
	TestNotNull(TEXT("The mock status node is reflected"), StatusFunction);
	if (StatusFunction)
	{
		TestTrue(TEXT("The status node explains Shipping behavior"),
			StatusFunction->GetToolTipText().ToString().Contains(
				TEXT("Shipping")));
	}
#endif

	Settings->DevelopmentInputMode = SavedMode;
	return true;
}

#endif

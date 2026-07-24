#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsSampleRecipes.h"

#if OPENMOBILE_HAPTICS_SAMPLE_SNAPSHOT_ENABLED
#include "OpenMobileHapticsCapabilityTester.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsSampleRecipesTest,
	"OpenMobile.Haptics.Sample.Recipes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSampleRecipesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	TestNull(
		TEXT("Null context has no subsystem"),
		UOpenMobileHapticsSampleRecipes::GetHapticsSubsystem(nullptr)
	);

	FOpenMobileHapticCapabilities Capabilities;
	TestFalse(
		TEXT("Unknown rich support does not select a rich recipe"),
		UOpenMobileHapticsSampleRecipes::HasRichHaptics(Capabilities)
	);
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
	TestTrue(
		TEXT("Supported rich capability selects a rich recipe"),
		UOpenMobileHapticsSampleRecipes::HasRichHaptics(Capabilities)
	);

	const FOpenMobileHapticPlaybackResult Prepared =
		UOpenMobileHapticsSampleRecipes::PlayPreparedPattern(
			nullptr,
			TEXT("Reward_Success"),
			0.5f,
			TEXT("Gameplay")
		);
	TestEqual(
		TEXT("Prepared recipe fails safely without a subsystem"),
		Prepared.Error.Code,
		EOpenMobileHapticErrorCode::BackendUnavailable
	);
	const FOpenMobileHapticPlaybackResult Vehicle =
		UOpenMobileHapticsSampleRecipes::StartBoundedVehicleFeedback(
			nullptr,
			0.45f
		);
	TestEqual(
		TEXT("Vehicle recipe fails safely without a subsystem"),
		Vehicle.Error.Code,
		EOpenMobileHapticErrorCode::BackendUnavailable
	);
	const FOpenMobileHapticPlaybackResult Audio =
		UOpenMobileHapticsSampleRecipes::ScheduleWithAudioClock(
			nullptr,
			TEXT("Reward_Success"),
			1.0,
			0.25
		);
	TestEqual(
		TEXT("Audio recipe fails safely without a subsystem"),
		Audio.Error.Code,
		EOpenMobileHapticErrorCode::BackendUnavailable
	);
	const FOpenMobileHapticControlResult Cancel =
		UOpenMobileHapticsSampleRecipes::CancelSamplePlayback(
			nullptr,
			{}
		);
	TestEqual(
		TEXT("Cancel recipe fails safely without a subsystem"),
		Cancel.Error.Code,
		EOpenMobileHapticErrorCode::BackendUnavailable
	);

#if OPENMOBILE_HAPTICS_SAMPLE_SNAPSHOT_ENABLED
	FOpenMobileHapticsCapabilityTesterSnapshot Snapshot;
	FString SnapshotJson;
	FString SnapshotError;
	TestFalse(
		TEXT("Snapshot export fails safely without a world"),
		UOpenMobileHapticsCapabilityTesterLibrary::
			CreateSanitizedCapabilitySnapshot(
				nullptr,
				Snapshot,
				SnapshotJson,
				SnapshotError
			)
	);
	TestTrue(
		TEXT("Snapshot export reports the missing subsystem"),
		!SnapshotError.IsEmpty()
	);
	TestTrue(
		TEXT("Failed snapshot export returns no JSON"),
		SnapshotJson.IsEmpty()
	);
#endif

	const UClass* RecipeClass = UOpenMobileHapticsSampleRecipes::StaticClass();
	for (const FName FunctionName : {
		FName(TEXT("GetHapticsSubsystem")),
		FName(TEXT("PlayPreparedPattern")),
		FName(TEXT("StartBoundedVehicleFeedback")),
		FName(TEXT("ScheduleWithAudioClock")),
		FName(TEXT("PlayAccessibilityConfirmation")),
		FName(TEXT("CancelSamplePlayback")),
		FName(TEXT("StopSampleChannel")),
		FName(TEXT("HasRichHaptics"))
	})
	{
		const UFunction* Function = RecipeClass->FindFunctionByName(FunctionName);
		TestNotNull(
			*FString::Printf(TEXT("%s is reflected"), *FunctionName.ToString()),
			Function
		);
		if (Function)
		{
			TestTrue(
				*FString::Printf(
					TEXT("%s is available to Blueprint"),
					*FunctionName.ToString()
				),
				Function->HasAnyFunctionFlags(
					FUNC_BlueprintCallable | FUNC_BlueprintPure
				)
			);
		}
	}
	return true;
}

#endif

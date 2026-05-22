#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsSubsystem.h"
#include "OpenMobileHapticsTypes.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsTypeDefaultsTest,
	"OpenMobile.Haptics.API.TypeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsTypeDefaultsTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const FOpenMobileHapticPlaybackHandle Handle;
	TestFalse(TEXT("Default playback handle is invalid"), Handle.IsValid());

	const FOpenMobileHapticCapabilities Capabilities;
	TestEqual(
		TEXT("Default capabilities report an unsupported platform"),
		Capabilities.Availability,
		EOpenMobileHapticAvailability::UnsupportedPlatform
	);
	TestEqual(
		TEXT("Unknown feature support remains explicit"),
		Capabilities.BasicVibration,
		EOpenMobileHapticSupportState::Unknown
	);

	const FOpenMobileHapticPlaybackOptions Options;
	TestEqual(
		TEXT("Default channel is Gameplay"),
		Options.Channel,
		FName(TEXT("Gameplay"))
	);
	TestEqual(
		TEXT("Default overlap replaces same-channel work"),
		Options.OverlapPolicy,
		EOpenMobileHapticOverlapPolicy::Replace
	);
	TestEqual(
		TEXT("Default schedule is immediate"),
		Options.Schedule.Mode,
		EOpenMobileHapticScheduleMode::Immediate
	);
	TestFalse(TEXT("Looping requires explicit opt-in"), Options.Loop.bLoop);

	const FOpenMobileHapticUserPolicy Policy;
	TestTrue(TEXT("Haptics are enabled by default"), Policy.bEnabled);
	TestEqual(TEXT("Master intensity defaults to one"), Policy.MasterIntensity, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsUnsupportedEditorTest,
	"OpenMobile.Haptics.API.UnsupportedEditor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsUnsupportedEditorTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	const FOpenMobileHapticCapabilities Capabilities =
		Subsystem->GetHapticCapabilities();
	TestEqual(
		TEXT("Editor capability is unsupported"),
		Capabilities.Availability,
		EOpenMobileHapticAvailability::UnsupportedPlatform
	);

	const TArray<FOpenMobileHapticPlaybackResult> Results = {
		Subsystem->PlaySemanticFeedback(
			EOpenMobileHapticSemanticEffect::Selection
		),
		Subsystem->Vibrate(),
		Subsystem->PlayNamedPattern(TEXT("UI_Confirm"))
	};
	for (const FOpenMobileHapticPlaybackResult& Result : Results)
	{
		TestEqual(
			TEXT("Unsupported play is rejected"),
			Result.Outcome,
			EOpenMobileHapticPlaybackOutcome::Rejected
		);
		TestEqual(
			TEXT("Unsupported play has a typed error"),
			Result.Error.Code,
			EOpenMobileErrorCode::NotSupported
		);
		TestFalse(
			TEXT("Rejected work has no playback handle"),
			Result.Handle.IsValid()
		);
	}

	const FOpenMobileHapticControlResult StopResult =
		Subsystem->StopPlayback({});
	TestEqual(
		TEXT("Unsupported stop has a typed error"),
		StopResult.Error.Code,
		EOpenMobileErrorCode::NotSupported
	);
	TestEqual(
		TEXT("Unknown handles remain invalid"),
		Subsystem->GetPlaybackState({}),
		EOpenMobileHapticPlaybackState::Invalid
	);

	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.MasterIntensity = std::numeric_limits<float>::quiet_NaN();
	const FOpenMobileHapticControlResult InvalidPolicy =
		Subsystem->SetUserPolicy(Policy);
	TestEqual(
		TEXT("Nonfinite policy is rejected"),
		InvalidPolicy.Error.Code,
		EOpenMobileErrorCode::InvalidArgument
	);

	Policy.MasterIntensity = 0.5f;
	const FOpenMobileHapticControlResult UpdatedPolicy =
		Subsystem->SetUserPolicy(Policy);
	TestEqual(
		TEXT("Valid local policy is accepted"),
		UpdatedPolicy.Outcome,
		EOpenMobileHapticControlOutcome::Accepted
	);
	TestEqual(
		TEXT("Accepted policy is retained"),
		Subsystem->GetUserPolicy().MasterIntensity,
		0.5f
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAsyncContractTest,
	"OpenMobile.Haptics.Async.ExactlyOnceAndTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAsyncContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	const UClass* ActionClass =
		UOpenMobileHapticPlaybackAsyncAction::StaticClass();
	for (const FName BranchName : {
		FName(TEXT("Completed")),
		FName(TEXT("Cancelled")),
		FName(TEXT("Failed"))
	})
	{
		const FMulticastDelegateProperty* Branch =
			FindFProperty<FMulticastDelegateProperty>(ActionClass, BranchName);
		TestTrue(
			*FString::Printf(
				TEXT("%s terminal branch is Blueprint assignable"),
				*BranchName.ToString()
			),
			Branch && Branch->HasAnyPropertyFlags(CPF_BlueprintAssignable)
		);
	}
	TestNotNull(
		TEXT("Async action exposes its playback handle"),
		ActionClass->FindPropertyByName(TEXT("PlaybackHandle"))
	);
	TestNotNull(
		TEXT("Async action exposes a Blueprint factory"),
		ActionClass->FindFunctionByName(TEXT("PlayNamedHapticAsync"))
	);

	int32 TerminalCount = 0;
	EOpenMobileHapticAsyncTerminalState LastState =
		EOpenMobileHapticAsyncTerminalState::Pending;
	UOpenMobileHapticPlaybackAsyncAction* Action =
		NewObject<UOpenMobileHapticPlaybackAsyncAction>();
	Action->OnNativeTerminal().AddLambda(
		[&TerminalCount, &LastState](
			EOpenMobileHapticAsyncTerminalState State,
			const FOpenMobileHapticPlaybackResult&)
		{
			++TerminalCount;
			LastState = State;
		}
	);
	FOpenMobileHapticPlaybackResult CompletedResult;
	CompletedResult.Outcome = EOpenMobileHapticPlaybackOutcome::Accepted;
	CompletedResult.State = EOpenMobileHapticPlaybackState::Completed;
	Action->FinishCompleted(CompletedResult);
	Action->FinishFailed(FOpenMobileHapticPlaybackResult::MakeRejected(
		EOpenMobileErrorCode::NativeFailure,
		TEXT("late failure")
	));
	Action->Cancel();
	TestEqual(TEXT("Only the first terminal path broadcasts"), TerminalCount, 1);
	TestEqual(
		TEXT("First terminal state wins"),
		LastState,
		EOpenMobileHapticAsyncTerminalState::Completed
	);

	int32 TeardownCancellationCount = 0;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* AsyncSubsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	UOpenMobileHapticPlaybackAsyncAction* TeardownAction =
		NewObject<UOpenMobileHapticPlaybackAsyncAction>();
	TeardownAction->OnNativeTerminal().AddLambda(
		[&TeardownCancellationCount](
			EOpenMobileHapticAsyncTerminalState State,
			const FOpenMobileHapticPlaybackResult&)
		{
			if (State == EOpenMobileHapticAsyncTerminalState::Cancelled)
			{
				++TeardownCancellationCount;
			}
		}
	);
	AsyncSubsystem->RegisterAsyncAction(TeardownAction);
	TeardownAction->Subsystem = AsyncSubsystem;
	AsyncSubsystem->Deinitialize();
	AsyncSubsystem->Deinitialize();
	TestEqual(
		TEXT("Game Instance teardown cancels exactly once"),
		TeardownCancellationCount,
		1
	);
	return true;
}

#endif

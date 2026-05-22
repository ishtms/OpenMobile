#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsSettings.h"
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
	FOpenMobileHapticsSettingsContractTest,
	"OpenMobile.Haptics.Settings.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSettingsContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticsSettings* Settings =
		NewObject<UOpenMobileHapticsSettings>();
	TestEqual(
		TEXT("Haptics settings use the OpenMobile category"),
		Settings->GetCategoryName(),
		FName(TEXT("OpenMobile"))
	);
	TestEqual(
		TEXT("Haptics settings keep their own section"),
		Settings->GetSectionName(),
		FName(TEXT("OpenMobile Haptics"))
	);
#if WITH_METADATA
	TestEqual(
		TEXT("Haptics display name matches its section"),
		Settings->GetClass()->GetMetaData(TEXT("DisplayName")),
		FString(TEXT("OpenMobile Haptics"))
	);
#endif
	TestTrue(
		TEXT("Haptics settings use default config"),
		Settings->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig)
	);
	const FFloatProperty* IntensityProperty = FindFProperty<FFloatProperty>(
		UOpenMobileHapticsSettings::StaticClass(),
		GET_MEMBER_NAME_CHECKED(
			UOpenMobileHapticsSettings,
			DefaultMasterIntensity
		)
	);
	TestTrue(
		TEXT("Master intensity is serialized to config"),
		IntensityProperty && IntensityProperty->HasAnyPropertyFlags(CPF_Config)
	);
	TestTrue(TEXT("Haptics are enabled by default"), Settings->bEnabledByDefault);
	TestEqual(
		TEXT("Default project intensity is one"),
		Settings->DefaultMasterIntensity,
		1.0f
	);
	TestEqual(TEXT("Five default channels are present"), Settings->Channels.Num(), 5);
	TestEqual(
		TEXT("Default channel is Gameplay"),
		Settings->DefaultChannel,
		FName(TEXT("Gameplay"))
	);
	TestTrue(
		TEXT("Custom Android vibration packaging is enabled by default"),
		Settings->Android.bPackageCustomVibration
	);

	TArray<FString> Errors;
	TestTrue(TEXT("Default settings validate"), Settings->Validate(Errors));
	TestTrue(TEXT("Default validation has no errors"), Errors.IsEmpty());

	Settings->DefaultMasterIntensity =
		std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite intensity is invalid"), Settings->Validate(Errors));
	Settings->DefaultMasterIntensity = 1.0f;

	FOpenMobileHapticChannelSettings DuplicateChannel = Settings->Channels[0];
	DuplicateChannel.Name = TEXT("ui");
	Settings->Channels.Add(DuplicateChannel);
	TestFalse(TEXT("Case-conflicting channels are invalid"), Settings->Validate(Errors));
	Settings->Channels.Pop();

	FOpenMobileHapticEffectSettings Effect;
	Effect.Name = TEXT("Confirm");
	Settings->EffectOverrides = {Effect, Effect};
	TestFalse(TEXT("Duplicate effects are invalid"), Settings->Validate(Errors));
	Settings->EffectOverrides.Reset();

	FOpenMobileHapticNamedLibrarySettings MissingLibrary;
	MissingLibrary.Name = TEXT("Missing");
	MissingLibrary.Asset = FSoftObjectPath(TEXT("/Game/Haptics/Missing.Missing"));
	Settings->NamedLibraries.Add(MissingLibrary);
	TestFalse(TEXT("Missing library assets are invalid"), Settings->Validate(Errors));
	Settings->NamedLibraries.Reset();

	Settings->MaximumFiniteRepeatCount = 0;
	TestFalse(TEXT("Zero loop limit is invalid"), Settings->Validate(Errors));
	Settings->MaximumFiniteRepeatCount = 32;

	Settings->BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::AllowAll;
	TestFalse(TEXT("Unrestricted background haptics are invalid"), Settings->Validate(Errors));
	Settings->BackgroundPolicy = EOpenMobileHapticBackgroundPolicy::StopAll;

	Settings->bEnableCustomPlayback = false;
	TestFalse(TEXT("Disabled custom playback cannot package Android vibration"), Settings->Validate(Errors));
	Settings->Android.bPackageCustomVibration = false;
	Settings->IOS.bEnableCoreHaptics = false;
	TestFalse(TEXT("Disabled Core Haptics cannot package AHAP"), Settings->Validate(Errors));
	Settings->IOS.bPackageAHAPResources = false;
	TestTrue(TEXT("Semantic-only platform settings validate"), Settings->Validate(Errors));

	const FString ConfigPath = FPaths::CreateTempFilename(
		*FPaths::ProjectIntermediateDir(),
		TEXT("OpenMobileHapticsSettings"),
		TEXT(".ini")
	);
	Settings->bEnabledByDefault = false;
	Settings->DefaultMasterIntensity = 0.75f;
	Settings->DefaultChannel = TEXT("UI");
	Settings->MaximumQueuedHandles = 12;
	Settings->SelectionDebounceSeconds = 0.06f;
	Settings->Channels[0].IntensityScale = 0.6f;
	Effect.IntensityScale = 0.8f;
	Settings->EffectOverrides.Add(Effect);
	Settings->NamedLibraries.Add(MissingLibrary);
	Settings->SaveConfig(CPF_Config, *ConfigPath, GConfig, false);
	UOpenMobileHapticsSettings* Loaded =
		NewObject<UOpenMobileHapticsSettings>();
	Loaded->LoadConfig(UOpenMobileHapticsSettings::StaticClass(), *ConfigPath);
	IFileManager::Get().Delete(*ConfigPath, false, true, true);
	TestFalse(
		TEXT("Enable default survives editor restart serialization"),
		Loaded->bEnabledByDefault
	);
	TestEqual(
		TEXT("Master intensity survives editor restart serialization"),
		Loaded->DefaultMasterIntensity,
		0.75f
	);
	TestEqual(
		TEXT("Default channel survives editor restart serialization"),
		Loaded->DefaultChannel,
		FName(TEXT("UI"))
	);
	TestEqual(
		TEXT("Queue limit survives editor restart serialization"),
		Loaded->MaximumQueuedHandles,
		12
	);
	TestEqual(
		TEXT("Rate limit survives editor restart serialization"),
		Loaded->SelectionDebounceSeconds,
		0.06f
	);
	TestFalse(
		TEXT("Android packaging override survives serialization"),
		Loaded->Android.bPackageCustomVibration
	);
	TestFalse(
		TEXT("Apple engine override survives serialization"),
		Loaded->IOS.bEnableCoreHaptics
	);
	TestEqual(
		TEXT("Channel defaults are not duplicated during reload"),
		Loaded->Channels.Num(),
		5
	);
	TestEqual(
		TEXT("Nested channel settings survive serialization"),
		Loaded->Channels[0].IntensityScale,
		0.6f
	);
	TestEqual(
		TEXT("Effect overrides survive serialization"),
		Loaded->EffectOverrides.Num(),
		1
	);
	TestEqual(
		TEXT("Named library paths survive serialization"),
		Loaded->NamedLibraries[0].Asset,
		MissingLibrary.Asset
	);
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

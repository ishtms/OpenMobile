#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "IOpenMobileHapticsBackend.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsErrorMapper.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsSubsystem.h"
#include "OpenMobileHapticsTypes.h"
#include "UObject/UnrealType.h"

namespace OpenMobileHapticsTests
{
	class FMockBackend final : public IOpenMobileHapticsBackend
	{
	public:
		explicit FMockBackend(FName InName, int32 InPriority = 0)
			: Name(InName)
			, Priority(InPriority)
		{
			Capabilities.BackendName = Name;
		}

		virtual FName GetBackendName() const override { return Name; }
		virtual int32 GetPriority() const override { return Priority; }
		virtual bool IsAvailable() const override { return bAvailable; }
		virtual FOpenMobileHapticCapabilities GetCapabilities() const override
		{
			return Capabilities;
		}
		virtual void HandleLifecycleChange() override
		{
			++LifecycleChangeCount;
			if (bApplyCapabilitiesAfterLifecycle)
			{
				Capabilities = CapabilitiesAfterLifecycle;
			}
		}
		virtual EOpenMobileHapticsBackendPreparationState
		GetPreparationState() const override
		{
			return PreparationState;
		}
		virtual FOpenMobileHapticsBackendControlSupport
		GetControlSupport() const override
		{
			return ControlSupport;
		}

		virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
			const FOpenMobileHapticSemanticRequest& Request,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			static_cast<void>(Request);
			++SemanticSubmissionCount;
			LastToken = Token;
			return MakeSubmission(false, false, MoveTemp(Callback));
		}

		virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
			const FOpenMobileHapticOneShotRequest& Request,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			static_cast<void>(Request);
			++OneShotSubmissionCount;
			LastToken = Token;
			return MakeSubmission(true, true, MoveTemp(Callback));
		}

		virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
			const FOpenMobileHapticNamedPatternRequest& Request,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			static_cast<void>(Request);
			++NamedSubmissionCount;
			LastToken = Token;
			return MakeSubmission(true, true, MoveTemp(Callback));
		}

		virtual FOpenMobileHapticControlResult StopPlayback(
			const FOpenMobileHapticsBackendRequestToken& Token
		) override
		{
			LastStoppedToken = Token;
			FOpenMobileHapticControlResult Result;
			Result.Outcome = ControlSupport.bStop
				? EOpenMobileHapticControlOutcome::Accepted
				: EOpenMobileHapticControlOutcome::Unsupported;
			return Result;
		}

		virtual void BeginShutdown() override
		{
			++ShutdownCount;
		}

		void Emit(
			int32 PendingIndex,
			EOpenMobileHapticPlaybackState State,
			uint64 Sequence
		)
		{
			FOpenMobileHapticsBackendCallback Callback;
			Callback.Token = PendingCallbacks[PendingIndex].Token;
			Callback.Sequence = Sequence;
			Callback.Event.Handle = Callback.Token.PlaybackHandle;
			Callback.Event.State = State;
			Callback.Event.TimestampSeconds = CurrentTimeSeconds;
			PendingCallbacks[PendingIndex].Callback(Callback);
		}

		FOpenMobileHapticCapabilities Capabilities;
		FOpenMobileHapticCapabilities CapabilitiesAfterLifecycle;
		EOpenMobileHapticsBackendPreparationState PreparationState =
			EOpenMobileHapticsBackendPreparationState::Unprepared;
		FOpenMobileHapticsBackendControlSupport ControlSupport;
		bool bAvailable = true;
		bool bFailSubmissions = false;
		bool bFailSubmissionsWithoutError = false;
		bool bApplyCapabilitiesAfterLifecycle = false;
		double CurrentTimeSeconds = 0.0;
		int32 SemanticSubmissionCount = 0;
		int32 OneShotSubmissionCount = 0;
		int32 NamedSubmissionCount = 0;
		int32 ShutdownCount = 0;
		int32 LifecycleChangeCount = 0;
		FOpenMobileHapticsBackendRequestToken LastToken;
		FOpenMobileHapticsBackendRequestToken LastStoppedToken;

	private:
		struct FPendingCallback
		{
			FOpenMobileHapticsBackendRequestToken Token;
			FOpenMobileHapticsBackendEventCallback Callback;
		};

		FOpenMobileHapticsBackendSubmission MakeSubmission(
			bool bControllable,
			bool bExpectsCallbacks,
			FOpenMobileHapticsBackendEventCallback Callback
		)
		{
			FOpenMobileHapticsBackendSubmission Submission;
			if (bFailSubmissionsWithoutError)
			{
				return Submission;
			}
			if (bFailSubmissions)
			{
				Submission.Result =
					FOpenMobileHapticPlaybackResult::MakeRejected(
						EOpenMobileErrorCode::NativeFailure,
						TEXT("Injected backend failure.")
					);
				return Submission;
			}

			Submission.Result.Outcome =
				EOpenMobileHapticPlaybackOutcome::Accepted;
			Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
			Submission.bCreatesControllablePlayback = bControllable;
			Submission.bExpectsCallbacks = bExpectsCallbacks;
			if (bExpectsCallbacks)
			{
				PendingCallbacks.Add({LastToken, MoveTemp(Callback)});
			}
			return Submission;
		}

		FName Name;
		int32 Priority = 0;
		TArray<FPendingCallback> PendingCallbacks;
	};
}

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
			EOpenMobileHapticErrorCode::UnsupportedFeature
		);
		TestEqual(
			TEXT("Unsupported play retains the common error"),
			Result.Error.CommonCode,
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
		EOpenMobileHapticErrorCode::UnsupportedFeature
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
		EOpenMobileHapticErrorCode::InvalidRequest
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBackendRegistryTest,
	"OpenMobile.Haptics.Backend.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBackendRegistryTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();

	FMockBackend Beta(TEXT("Beta"), 10);
	FMockBackend Alpha(TEXT("Alpha"), 10);
	FMockBackend Higher(TEXT("Higher"), 20);
	TestTrue(
		TEXT("First backend registers"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Beta)
	);
	TestTrue(
		TEXT("Equal-priority backend registers"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Alpha)
	);
	TestTrue(
		TEXT("Backend name breaks priority ties deterministically"),
		FOpenMobileHapticsBackendRegistry::FindBackend() == &Alpha
	);

	const FOpenMobileHapticsBackendRequestToken AlphaToken =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(Alpha, true);
	TestTrue(
		TEXT("Captured request token starts current"),
		FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(AlphaToken)
	);
	TestTrue(
		TEXT("Higher-priority backend registers"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Higher)
	);
	TestTrue(
		TEXT("Backend resolves lazily to the higher priority"),
		FOpenMobileHapticsBackendRegistry::FindBackend() == &Higher
	);
	TestFalse(
		TEXT("Registration invalidates stale callback tokens"),
		FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(AlphaToken)
	);

	FMockBackend Duplicate(TEXT("Higher"), 100);
	TestFalse(
		TEXT("Duplicate backend identity is rejected"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Duplicate)
	);
	TestTrue(
		TEXT("Backend unregisters"),
		FOpenMobileHapticsBackendRegistry::UnregisterBackend(Higher)
	);
	TestEqual(
		TEXT("Unregister begins shutdown before destruction"),
		Higher.ShutdownCount,
		1
	);
	TestTrue(
		TEXT("Tie-break remains stable after removal"),
		FOpenMobileHapticsBackendRegistry::FindBackend() == &Alpha
	);

	FOpenMobileHapticsBackendRegistry::BeginShutdown();
	TestEqual(TEXT("Alpha shuts down once"), Alpha.ShutdownCount, 1);
	TestEqual(TEXT("Beta shuts down once"), Beta.ShutdownCount, 1);
	TestNull(
		TEXT("No backend resolves during shutdown"),
		FOpenMobileHapticsBackendRegistry::FindBackend()
	);
	TestFalse(
		TEXT("Registration is rejected during shutdown"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Duplicate)
	);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Alpha);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Beta);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAvailabilityTest,
	"OpenMobile.Haptics.Capabilities.AvailabilityAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAvailabilityTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	TestEqual(
		TEXT("No backend remains an unsupported platform"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::UnsupportedPlatform
	);

	FMockBackend Backend(TEXT("Availability"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::NoActuator;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	TestEqual(
		TEXT("No actuator remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::NoActuator
	);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	TestEqual(
		TEXT("Basic-only hardware remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::BasicVibration
	);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	TestEqual(
		TEXT("Semantic hardware remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::SemanticFeedback
	);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	TestEqual(
		TEXT("Rich hardware remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::RichHaptics
	);

	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.bEnabled = false;
	Subsystem->SetUserPolicy(Policy);
	TestEqual(
		TEXT("Player policy overrides hardware availability"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::DisabledByPolicy
	);
	Policy.bEnabled = true;
	Subsystem->SetUserPolicy(Policy);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::TemporarilyUnavailable;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	Backend.CapabilitiesAfterLifecycle.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.bApplyCapabilitiesAfterLifecycle = true;
	TestEqual(
		TEXT("Temporary engine state remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::TemporarilyUnavailable
	);
	FOpenMobileHapticsBackendRegistry::NotifyLifecycleChange();
	TestEqual(
		TEXT("Lifecycle refresh reaches the backend"),
		Backend.LifecycleChangeCount,
		1
	);
	TestEqual(
		TEXT("Engine state refreshes after lifecycle change"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::RichHaptics
	);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsDetailedCapabilityTest,
	"OpenMobile.Haptics.Capabilities.DetailedProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsDetailedCapabilityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FOpenMobileHapticCapabilities Unknown;
	TestEqual(
		TEXT("Amplitude support defaults to unknown"),
		Unknown.AmplitudeControl,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Envelope support defaults to unknown"),
		Unknown.Envelopes,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Frequency support defaults to unknown"),
		Unknown.FrequencyControl,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Audio event support defaults to unknown"),
		Unknown.AudioEvents,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Seek support defaults to unknown"),
		Unknown.Seek,
		EOpenMobileHapticSupportState::Unknown
	);
	TestFalse(
		TEXT("Maximum event count defaults to unknown"),
		Unknown.MaximumEventCount.bKnown
	);
	TestFalse(
		TEXT("Minimum timing granularity defaults to unknown"),
		Unknown.MinimumTimingGranularitySeconds.bKnown
	);

	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("Detailed"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Primitives =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Envelopes =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.PrimitiveSupport = {
		{TEXT("Click"), EOpenMobileHapticSupportState::Supported},
		{TEXT("Spin"), EOpenMobileHapticSupportState::Unsupported}
	};
	Backend.Capabilities.PresetSupport = {
		{TEXT("Tick"), EOpenMobileHapticSupportState::Supported},
		{TEXT("HeavyClick"), EOpenMobileHapticSupportState::Unknown}
	};
	Backend.Capabilities.MaximumEventCount = {true, 128};
	Backend.Capabilities.MaximumControlPointCount = {true, 16};
	Backend.Capabilities.MaximumDurationSeconds = {true, 30.0};
	Backend.Capabilities.MaximumQueueDepth = {true, 8};
	Backend.Capabilities.MinimumTimingGranularitySeconds = {true, 0.001};
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const FOpenMobileHapticCapabilities First =
		Subsystem->GetCapabilitiesNative();
	TestEqual(
		TEXT("Per-primitive support is retained"),
		First.PrimitiveSupport.Num(),
		2
	);
	TestEqual(
		TEXT("Per-preset unknown state is retained"),
		First.PresetSupport[1].Support,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Known event limit is retained"),
		First.MaximumEventCount.Value,
		128
	);
	TestEqual(
		TEXT("Known duration limit is retained"),
		First.MaximumDurationSeconds.Seconds,
		30.0
	);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::NoActuator;
	TestEqual(
		TEXT("Published snapshot is immutable between refreshes"),
		Subsystem->GetCapabilitiesNative().Availability,
		EOpenMobileHapticAvailability::RichHaptics
	);
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	TestEqual(
		TEXT("Explicit refresh publishes the new profile"),
		Subsystem->GetCapabilitiesNative().Availability,
		EOpenMobileHapticAvailability::NoActuator
	);

	TFuture<FOpenMobileHapticCapabilities> Future = Async(
		EAsyncExecution::ThreadPool,
		[Subsystem]()
		{
			return Subsystem->GetCapabilitiesNative();
		}
	);
	const FOpenMobileHapticCapabilities BackgroundSnapshot = Future.Get();
	TestEqual(
		TEXT("Background readers receive the immutable snapshot"),
		BackgroundSnapshot.BackendName,
		FName(TEXT("Detailed"))
	);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBackendSubmissionTest,
	"OpenMobile.Haptics.Backend.SubmissionAndCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBackendSubmissionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();

	FMockBackend Low(TEXT("Low"), 1);
	Low.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	Low.PreparationState =
		EOpenMobileHapticsBackendPreparationState::Preparing;
	FMockBackend High(TEXT("High"), 10);
	High.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	High.PreparationState =
		EOpenMobileHapticsBackendPreparationState::Prepared;
	High.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Low);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	TestEqual(
		TEXT("Capability query uses the current backend"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::BasicVibration
	);
	const FOpenMobileHapticPlaybackResult LowSemantic =
		Subsystem->PlaySemanticFeedback(
			EOpenMobileHapticSemanticEffect::Selection
		);
	TestTrue(TEXT("Semantic request is accepted"), LowSemantic.IsAccepted());
	TestFalse(
		TEXT("Fire-and-forget semantic request has no handle"),
		LowSemantic.Handle.IsValid()
	);
	TestTrue(TEXT("Request ID crosses the backend seam"), Low.LastToken.RequestId > 0);
	TestFalse(
		TEXT("Semantic request has no playback ID"),
		Low.LastToken.PlaybackHandle.IsValid()
	);

	FOpenMobileHapticsBackendRegistry::RegisterBackend(High);
	Subsystem->PlaySemanticFeedback(EOpenMobileHapticSemanticEffect::Click);
	TestEqual(
		TEXT("Next operation resolves the replacement backend"),
		High.SemanticSubmissionCount,
		1
	);
	TestEqual(
		TEXT("Backend preparation state is reported"),
		FOpenMobileHapticsBackendRegistry::FindBackend()->GetPreparationState(),
		EOpenMobileHapticsBackendPreparationState::Prepared
	);

	int32 EventCount = 0;
	EOpenMobileHapticPlaybackState LastState =
		EOpenMobileHapticPlaybackState::Invalid;
	double LastTimestamp = 0.0;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&EventCount, &LastState, &LastTimestamp](
			const FOpenMobileHapticPlaybackEvent& Event
		)
		{
			++EventCount;
			LastState = Event.State;
			LastTimestamp = Event.TimestampSeconds;
		}
	);
	const FOpenMobileHapticPlaybackResult Named =
		Subsystem->PlayNamedPattern(TEXT("UI_Confirm"));
	TestTrue(TEXT("Named request is accepted"), Named.IsAccepted());
	TestTrue(TEXT("Controllable request has a handle"), Named.Handle.IsValid());
	TestEqual(
		TEXT("Accepted handle state is retained"),
		Subsystem->GetPlaybackState(Named.Handle),
		EOpenMobileHapticPlaybackState::Accepted
	);

	const FOpenMobileHapticControlResult Stop =
		Subsystem->StopPlayback(Named.Handle);
	TestEqual(
		TEXT("Supported stop routes to the owning backend"),
		Stop.Outcome,
		EOpenMobileHapticControlOutcome::Accepted
	);
	TestEqual(
		TEXT("Stop preserves request identity"),
		High.LastStoppedToken.RequestId,
		High.LastToken.RequestId
	);
	TestEqual(
		TEXT("Stop preserves playback identity"),
		High.LastStoppedToken.PlaybackHandle,
		Named.Handle
	);

	High.CurrentTimeSeconds = 12.5;
	High.Emit(0, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Delayed callback is delivered"), EventCount, 1);
	TestEqual(
		TEXT("Callback state is preserved"),
		LastState,
		EOpenMobileHapticPlaybackState::Started
	);
	TestEqual(TEXT("Mock time is preserved"), LastTimestamp, 12.5);
	High.Emit(0, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Duplicate callback is ignored"), EventCount, 1);

	High.Emit(0, EOpenMobileHapticPlaybackState::Completed, 2);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Terminal callback is delivered once"), EventCount, 2);
	TestEqual(
		TEXT("Completed state remains queryable"),
		Subsystem->GetPlaybackState(Named.Handle),
		EOpenMobileHapticPlaybackState::Completed
	);
	High.Emit(0, EOpenMobileHapticPlaybackState::Completed, 3);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Post-terminal callback is ignored"), EventCount, 2);

	const FOpenMobileHapticPlaybackResult Stale =
		Subsystem->PlayNamedPattern(TEXT("Stale"));
	FMockBackend Newest(TEXT("Newest"), 20);
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Newest);
	High.Emit(1, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Stale backend callback is ignored"), EventCount, 2);
	TestTrue(TEXT("Stale request originally had a handle"), Stale.Handle.IsValid());

	Newest.bFailSubmissions = true;
	const FOpenMobileHapticPlaybackResult Failed =
		Subsystem->PlayNamedPattern(TEXT("Failure"));
	TestEqual(
		TEXT("Injected native failure remains typed"),
		Failed.Error.Code,
		EOpenMobileHapticErrorCode::NativeEngineFailure
	);
	TestEqual(
		TEXT("Injected native failure retains the common code"),
		Failed.Error.CommonCode,
		EOpenMobileErrorCode::NativeFailure
	);
	TestFalse(TEXT("Failed submission has no handle"), Failed.Handle.IsValid());

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Newest);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(High);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Low);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsErrorMappingTest,
	"OpenMobile.Haptics.Errors.MappingAndRedaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsErrorMappingTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	struct FExpectedMapping
	{
		EOpenMobileHapticsFailureReason Reason;
		EOpenMobileHapticErrorCode HapticCode;
		EOpenMobileErrorCode CommonCode;
	};
	const TArray<FExpectedMapping> Mappings = {
		{EOpenMobileHapticsFailureReason::UnsupportedHardware,
			EOpenMobileHapticErrorCode::UnsupportedHardware,
			EOpenMobileErrorCode::NotSupported},
		{EOpenMobileHapticsFailureReason::UnsupportedFeature,
			EOpenMobileHapticErrorCode::UnsupportedFeature,
			EOpenMobileErrorCode::NotSupported},
		{EOpenMobileHapticsFailureReason::DisabledPolicy,
			EOpenMobileHapticErrorCode::DisabledByPolicy,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileHapticsFailureReason::InvalidPattern,
			EOpenMobileHapticErrorCode::InvalidPattern,
			EOpenMobileErrorCode::InvalidArgument},
		{EOpenMobileHapticsFailureReason::RateLimited,
			EOpenMobileHapticErrorCode::RateLimited,
			EOpenMobileErrorCode::Busy},
		{EOpenMobileHapticsFailureReason::BusyChannel,
			EOpenMobileHapticErrorCode::ChannelBusy,
			EOpenMobileErrorCode::Busy},
		{EOpenMobileHapticsFailureReason::LifecycleRestricted,
			EOpenMobileHapticErrorCode::LifecycleRestricted,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileHapticsFailureReason::NativeEngineFailure,
			EOpenMobileHapticErrorCode::NativeEngineFailure,
			EOpenMobileErrorCode::NativeFailure},
		{EOpenMobileHapticsFailureReason::Interrupted,
			EOpenMobileHapticErrorCode::Interrupted,
			EOpenMobileErrorCode::NativeFailure}
	};
	for (const FExpectedMapping& Mapping : Mappings)
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = Mapping.Reason;
		Context.Stage = EOpenMobileHapticFailureStage::Playback;
		const FOpenMobileHapticError Error =
			FOpenMobileHapticsErrorMapper::Map(Context);
		TestEqual(TEXT("Known reason maps to a Haptics code"), Error.Code, Mapping.HapticCode);
		TestEqual(TEXT("Known reason maps to a common code"), Error.CommonCode, Mapping.CommonCode);
		TestFalse(TEXT("Mapped error has a useful message"), Error.Message.IsEmpty());
		TestFalse(TEXT("Mapped error has a correction"), Error.Correction.IsEmpty());
		TestTrue(TEXT("Pre-submission mapping is marked rejected"), Error.bRejectedBeforeSubmission);
	}

	FOpenMobileHapticsErrorContext FutureNative;
	FutureNative.Reason = EOpenMobileHapticsFailureReason::NativeEngineFailure;
	FutureNative.Stage = EOpenMobileHapticFailureStage::NativeSubmission;
	FutureNative.NativeDomain = TEXT("CoreHaptics.Engine");
	FutureNative.NativeCode = TEXT("FutureEngine_4097");
	FutureNative.bAfterAcceptance = true;
	const FOpenMobileHapticError FutureError =
		FOpenMobileHapticsErrorMapper::Map(FutureNative);
	TestEqual(
		TEXT("Future native domain is preserved"),
		FutureError.NativeDomain,
		FString(TEXT("CoreHaptics.Engine"))
	);
	TestEqual(
		TEXT("Future native code is preserved"),
		FutureError.NativeCode,
		FString(TEXT("FutureEngine_4097"))
	);
	TestFalse(
		TEXT("Post-acceptance failure is not a rejection"),
		FutureError.bRejectedBeforeSubmission
	);
	TestFalse(
		TEXT("Missing native messages use the mapped message"),
		FutureError.Message.IsEmpty()
	);

	FOpenMobileHapticsErrorContext Malformed;
	Malformed.Reason = EOpenMobileHapticsFailureReason::InvalidPattern;
	Malformed.Stage = EOpenMobileHapticFailureStage::Compilation;
	Malformed.FailedItem = TEXT("BrokenPattern");
	Malformed.Channel = TEXT("Gameplay");
	Malformed.FallbackAttempts = {TEXT("PortableRich"), TEXT("BasicVibration")};
	const FOpenMobileHapticError MalformedError =
		FOpenMobileHapticsErrorMapper::Map(Malformed);
	TestEqual(
		TEXT("Malformed asset keeps its failed stage"),
		MalformedError.Stage,
		EOpenMobileHapticFailureStage::Compilation
	);
	TestEqual(
		TEXT("Malformed asset keeps its safe identity"),
		MalformedError.FailedItem,
		FName(TEXT("BrokenPattern"))
	);
	TestEqual(
		TEXT("Fallback attempts remain available"),
		MalformedError.FallbackAttempts.Num(),
		2
	);

	FOpenMobileHapticsErrorContext Sensitive;
	Sensitive.Reason = EOpenMobileHapticsFailureReason::NativeEngineFailure;
	Sensitive.Stage = EOpenMobileHapticFailureStage::Playback;
	Sensitive.NativeDomain = TEXT("/Users/player/private/CoreHaptics");
	Sensitive.NativeCode = TEXT("token\nsecret");
	const FOpenMobileHapticError Redacted =
		FOpenMobileHapticsErrorMapper::Map(Sensitive);
	TestEqual(
		TEXT("Unsafe native domain is redacted"),
		Redacted.NativeDomain,
		FString(TEXT("redacted"))
	);
	TestEqual(
		TEXT("Unsafe native code is redacted"),
		Redacted.NativeCode,
		FString(TEXT("redacted"))
	);
	TestFalse(
		TEXT("Generated message does not contain a file path"),
		Redacted.Message.Contains(TEXT("/Users/"))
	);

	FOpenMobileHapticsErrorContext Interrupted;
	Interrupted.Reason = EOpenMobileHapticsFailureReason::Interrupted;
	Interrupted.Stage = EOpenMobileHapticFailureStage::Interruption;
	Interrupted.bAfterAcceptance = true;
	const FOpenMobileHapticError InterruptedError =
		FOpenMobileHapticsErrorMapper::Map(Interrupted);
	TestTrue(
		TEXT("Accepted interruption is distinguished"),
		InterruptedError.bInterruptedAfterAcceptance
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsMissingBackendErrorTest,
	"OpenMobile.Haptics.Errors.MissingBackendDetails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsMissingBackendErrorTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("MissingDetails"));
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	Backend.bFailSubmissionsWithoutError = true;
	const FOpenMobileHapticPlaybackResult Rejected =
		Subsystem->PlayNamedPattern(TEXT("MissingSubmissionError"));
	TestEqual(
		TEXT("Missing submission errors become native failures"),
		Rejected.Error.Code,
		EOpenMobileHapticErrorCode::NativeEngineFailure
	);
	TestEqual(
		TEXT("Missing submission errors identify their stage"),
		Rejected.Error.Stage,
		EOpenMobileHapticFailureStage::NativeSubmission
	);
	TestFalse(
		TEXT("Missing submission messages receive a useful message"),
		Rejected.Error.Message.IsEmpty()
	);
	TestTrue(
		TEXT("Missing submission errors remain pre-acceptance"),
		Rejected.Error.bRejectedBeforeSubmission
	);

	Backend.bFailSubmissionsWithoutError = false;
	const FOpenMobileHapticPlaybackResult Accepted =
		Subsystem->PlayNamedPattern(TEXT("MissingCallbackError"));
	FOpenMobileHapticError TerminalError;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&TerminalError](const FOpenMobileHapticPlaybackEvent& Event)
		{
			TerminalError = Event.Error;
		}
	);
	Backend.Emit(0, EOpenMobileHapticPlaybackState::Failed, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestTrue(
		TEXT("Accepted request has a valid handle"),
		Accepted.Handle.IsValid()
	);
	TestEqual(
		TEXT("Missing terminal errors become native failures"),
		TerminalError.Code,
		EOpenMobileHapticErrorCode::NativeEngineFailure
	);
	TestEqual(
		TEXT("Missing terminal errors identify playback"),
		TerminalError.Stage,
		EOpenMobileHapticFailureStage::Playback
	);
	TestFalse(
		TEXT("Missing terminal messages receive a useful message"),
		TerminalError.Message.IsEmpty()
	);
	TestFalse(
		TEXT("Terminal failures remain post-acceptance"),
		TerminalError.bRejectedBeforeSubmission
	);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

#endif

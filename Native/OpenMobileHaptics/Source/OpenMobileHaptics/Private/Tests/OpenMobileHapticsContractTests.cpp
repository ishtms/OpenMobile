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
#include "OpenMobileHapticLibrary.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsRateLimiter.h"
#include "OpenMobileHapticsSemanticPolicy.h"
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
			const FOpenMobileHapticsSemanticResolution& Resolution,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			LastSemanticRequest = Request;
			LastSemanticResolution = Resolution;
			++SemanticSubmissionCount;
			LastToken = Token;
			if (bNativePolicySuppressesSemantic)
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Suppressed;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Completed;
				Submission.Result.ResolvedPath = TEXT("SystemSemantic");
				return Submission;
			}
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
			LastNamedRequest = Request;
			++NamedSubmissionCount;
			LastToken = Token;
			if (bFailNamedSubmissions)
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result =
					FOpenMobileHapticPlaybackResult::MakeRejected(
						EOpenMobileErrorCode::NotSupported,
						TEXT("Injected named-pattern failure.")
					);
				return Submission;
			}
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
		bool bNativePolicySuppressesSemantic = false;
		bool bFailNamedSubmissions = false;
		bool bApplyCapabilitiesAfterLifecycle = false;
		double CurrentTimeSeconds = 0.0;
		int32 SemanticSubmissionCount = 0;
		int32 OneShotSubmissionCount = 0;
		int32 NamedSubmissionCount = 0;
		int32 ShutdownCount = 0;
		int32 LifecycleChangeCount = 0;
		FOpenMobileHapticsBackendRequestToken LastToken;
		FOpenMobileHapticsBackendRequestToken LastStoppedToken;
		FOpenMobileHapticSemanticRequest LastSemanticRequest;
		FOpenMobileHapticNamedPatternRequest LastNamedRequest;
		FOpenMobileHapticsSemanticResolution LastSemanticResolution;

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
	FOpenMobileHapticsSemanticPolicyTest,
	"OpenMobile.Haptics.Semantic.PolicyAndMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSemanticPolicyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	struct FExpectedMapping
	{
		EOpenMobileHapticSemanticEffect Effect;
		EOpenMobileHapticsSemanticBehavior Behavior;
		FName Name;
		FName Category;
	};
	const FExpectedMapping Mappings[] = {
		{EOpenMobileHapticSemanticEffect::Selection,
			EOpenMobileHapticsSemanticBehavior::Selection,
			TEXT("Selection"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactLight,
			EOpenMobileHapticsSemanticBehavior::ImpactLight,
			TEXT("ImpactLight"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactMedium,
			EOpenMobileHapticsSemanticBehavior::ImpactMedium,
			TEXT("ImpactMedium"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactHeavy,
			EOpenMobileHapticsSemanticBehavior::ImpactHeavy,
			TEXT("ImpactHeavy"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactSoft,
			EOpenMobileHapticsSemanticBehavior::ImpactSoft,
			TEXT("ImpactSoft"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactRigid,
			EOpenMobileHapticsSemanticBehavior::ImpactRigid,
			TEXT("ImpactRigid"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::NotificationSuccess,
			EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("NotificationSuccess"), TEXT("Alerts")},
		{EOpenMobileHapticSemanticEffect::NotificationWarning,
			EOpenMobileHapticsSemanticBehavior::NotificationWarning,
			TEXT("NotificationWarning"), TEXT("Alerts")},
		{EOpenMobileHapticSemanticEffect::NotificationError,
			EOpenMobileHapticsSemanticBehavior::NotificationError,
			TEXT("NotificationError"), TEXT("Alerts")},
		{EOpenMobileHapticSemanticEffect::Confirm,
			EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("Confirm"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::Reject,
			EOpenMobileHapticsSemanticBehavior::NotificationError,
			TEXT("Reject"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::Tick,
			EOpenMobileHapticsSemanticBehavior::Selection,
			TEXT("Tick"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::Click,
			EOpenMobileHapticsSemanticBehavior::ImpactLight,
			TEXT("Click"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::Bump,
			EOpenMobileHapticsSemanticBehavior::ImpactMedium,
			TEXT("Bump"), TEXT("Gameplay")},
		{EOpenMobileHapticSemanticEffect::Damage,
			EOpenMobileHapticsSemanticBehavior::ImpactHeavy,
			TEXT("Damage"), TEXT("Gameplay")},
		{EOpenMobileHapticSemanticEffect::Pickup,
			EOpenMobileHapticsSemanticBehavior::ImpactSoft,
			TEXT("Pickup"), TEXT("Gameplay")},
		{EOpenMobileHapticSemanticEffect::Achievement,
			EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("Achievement"), TEXT("Alerts")}
	};
	for (const FExpectedMapping& Mapping : Mappings)
	{
		const FOpenMobileHapticsSemanticDescriptor Descriptor =
			FOpenMobileHapticsSemanticPolicy::Describe(Mapping.Effect);
		TestEqual(TEXT("Semantic behavior is stable"), Descriptor.Behavior,
			Mapping.Behavior);
		TestEqual(TEXT("Semantic name is stable"), Descriptor.Name,
			Mapping.Name);
		TestEqual(TEXT("Semantic category is stable"), Descriptor.Category,
			Mapping.Category);
	}

	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	TestEqual(
		TEXT("System semantic behavior is preferred"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Path,
		EOpenMobileHapticsSemanticPath::SystemSemantic
	);
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Supported;
	const FOpenMobileHapticsSemanticResolution Predefined =
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		);
	TestEqual(TEXT("Predefined fallback is selected"), Predefined.Path,
		EOpenMobileHapticsSemanticPath::PredefinedEffect);
	TestTrue(TEXT("Predefined path is reported as fallback"),
		Predefined.bFallback);
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	TestEqual(
		TEXT("Basic vibration is the last active fallback"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Path,
		EOpenMobileHapticsSemanticPath::BasicVibration
	);
	TestEqual(
		TEXT("No-basic policy rejects the basic fallback"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::NoBasicVibration
		).Path,
		EOpenMobileHapticsSemanticPath::Unsupported
	);
	TestEqual(
		TEXT("Exact-only policy rejects portable fallbacks"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::ExactOnly
		).Path,
		EOpenMobileHapticsSemanticPath::Unsupported
	);
	TestTrue(
		TEXT("No-effect policy suppresses unavailable feedback"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::NoEffectAllowed
		).bSuppressWhenUnavailable
	);
	FOpenMobileHapticCapabilities AndroidProfile;
	AndroidProfile.BackendName = TEXT("AndroidMock");
	AndroidProfile.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	AndroidProfile.PredefinedEffects = EOpenMobileHapticSupportState::Supported;
	TestEqual(TEXT("Android profile prefers view semantics"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			AndroidProfile,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Path,
		EOpenMobileHapticsSemanticPath::SystemSemantic);
	FOpenMobileHapticCapabilities AppleProfile;
	AppleProfile.BackendName = TEXT("AppleMock");
	AppleProfile.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	AppleProfile.RichHaptics = EOpenMobileHapticSupportState::Supported;
	TestEqual(TEXT("Apple profile prefers UIKit semantics"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			AppleProfile,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Path,
		EOpenMobileHapticsSemanticPath::SystemSemantic);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsSemanticSubmissionPolicyTest,
	"OpenMobile.Haptics.Semantic.SubmissionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSemanticSubmissionPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);

	FMockBackend Backend(TEXT("SemanticMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const UFunction* SelectionFunction =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("PlaySelectionFeedback")
		);
	TestNotNull(TEXT("Selection feedback has a Blueprint node"),
		SelectionFunction);
	TestTrue(TEXT("Selection feedback node is Blueprint callable"),
		SelectionFunction
			&& SelectionFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	const FOpenMobileHapticPlaybackResult DedicatedSelection =
		Subsystem->PlaySelectionFeedback();
	TestTrue(TEXT("Dedicated selection entry point reaches the backend"),
		DedicatedSelection.IsAccepted());
	TestEqual(TEXT("Dedicated selection entry point uses selection semantics"),
		Backend.LastSemanticRequest.Effect,
		EOpenMobileHapticSemanticEffect::Selection);

	FOpenMobileHapticUserPolicy Policy;
	Policy.MasterIntensity = 0.8f;
	Policy.CategoryScales.Add(TEXT("Alerts"), 0.5f);
	Policy.EffectScales.Add(TEXT("NotificationWarning"), 0.25f);
	Subsystem->SetUserPolicy(Policy);
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = TEXT("AlertsPolicyTest");
	Options.Category = TEXT("Alerts");
	const FOpenMobileHapticPlaybackResult Scaled = Subsystem->SubmitSemantic({
		EOpenMobileHapticSemanticEffect::NotificationWarning,
		0.5f,
		Options
	});
	TestEqual(TEXT("Supported semantic request is accepted"), Scaled.Outcome,
		EOpenMobileHapticPlaybackOutcome::Accepted);
	TestEqual(TEXT("System semantic path is reported"), Scaled.ResolvedPath,
		FName(TEXT("SystemSemantic")));
	TestTrue(TEXT("Player scales are applied before submission"),
		FMath::IsNearlyEqual(Backend.LastSemanticRequest.Intensity, 0.05f));

	const int32 SubmittedBeforeRejections = Backend.SemanticSubmissionCount;
	FOpenMobileHapticSemanticRequest InvalidRequest;
	InvalidRequest.Intensity = std::numeric_limits<float>::quiet_NaN();
	const FOpenMobileHapticPlaybackResult Invalid =
		Subsystem->SubmitSemantic(InvalidRequest);
	TestEqual(TEXT("Nonfinite semantic intensity is rejected"),
		Invalid.Error.Code, EOpenMobileHapticErrorCode::InvalidRequest);

	Policy.bEnabled = false;
	Subsystem->SetUserPolicy(Policy);
	const FOpenMobileHapticPlaybackResult Disabled =
		Subsystem->PlaySelectionFeedback();
	TestEqual(TEXT("Disabled player policy suppresses feedback"),
		Disabled.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);

	Policy.bEnabled = true;
	Policy.CategoryScales.Add(TEXT("UI"), 0.0f);
	Subsystem->SetUserPolicy(Policy);
	const FOpenMobileHapticPlaybackResult ZeroScale =
		Subsystem->PlaySemanticFeedback(EOpenMobileHapticSemanticEffect::Click);
	TestEqual(TEXT("Zero effective intensity suppresses feedback"),
		ZeroScale.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);

	Policy.CategoryScales.Remove(TEXT("UI"));
	Subsystem->SetUserPolicy(Policy);
	UOpenMobileHapticsSettings* MutableSettings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const EOpenMobileHapticBackgroundPolicy SavedBackgroundPolicy =
		MutableSettings->BackgroundPolicy;
	MutableSettings->BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::CriticalOnly;
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(false);
	const FOpenMobileHapticPlaybackResult Background =
		Subsystem->PlaySemanticFeedback(EOpenMobileHapticSemanticEffect::Confirm);
	TestEqual(TEXT("Background semantic feedback is suppressed"),
		Background.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Noncritical background work never reaches the backend"),
		Backend.SemanticSubmissionCount, SubmittedBeforeRejections);
	Options.Channel = TEXT("CriticalBackgroundTest");
	Options.Category = TEXT("Alerts");
	Options.Priority = EOpenMobileHapticChannelPriority::Critical;
	const FOpenMobileHapticPlaybackResult CriticalBackground =
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::NotificationWarning,
			1.0f,
			Options
		});
	TestTrue(TEXT("Explicit critical policy permits critical background work"),
		CriticalBackground.IsAccepted());
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);
	MutableSettings->BackgroundPolicy = SavedBackgroundPolicy;
	Backend.bNativePolicySuppressesSemantic = true;
	Options.Channel = TEXT("MissingPresenterTest");
	Options.Category = TEXT("UI");
	Options.Priority = EOpenMobileHapticChannelPriority::Normal;
	const FOpenMobileHapticPlaybackResult MissingPresenter =
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::Selection,
			1.0f,
			Options
		});
	TestEqual(TEXT("Known native policy suppression is a nonfailure"),
		MissingPresenter.Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestFalse(TEXT("Native policy suppression does not invent an error"),
		MissingPresenter.Error.IsSet());
	Backend.bNativePolicySuppressesSemantic = false;

	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	Options.Channel = TEXT("FallbackPolicyTest");
	Options.Category = TEXT("Gameplay");
	const FOpenMobileHapticPlaybackResult Fallback = Subsystem->SubmitSemantic({
		EOpenMobileHapticSemanticEffect::Damage,
		1.0f,
		Options
	});
	TestEqual(TEXT("Portable fallback is reported"), Fallback.Outcome,
		EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Portable fallback path is reported"), Fallback.ResolvedPath,
		FName(TEXT("PredefinedEffect")));
	TestEqual(TEXT("Resolved fallback reaches the backend"),
		Backend.LastSemanticResolution.Path,
		EOpenMobileHapticsSemanticPath::PredefinedEffect);

	Options.Channel = TEXT("ExactPolicyTest");
	Options.FallbackPolicy = EOpenMobileHapticFallbackPolicy::ExactOnly;
	const FOpenMobileHapticPlaybackResult Unsupported =
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::Damage,
			1.0f,
			Options
		});
	TestEqual(TEXT("Unavailable exact feedback is typed as unsupported"),
		Unsupported.Error.Code,
		EOpenMobileHapticErrorCode::UnsupportedFeature);

	UOpenMobileHapticsSubsystem* DebounceSubsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const FOpenMobileHapticPlaybackResult FirstSelection =
		DebounceSubsystem->PlaySemanticFeedback(
			EOpenMobileHapticSemanticEffect::Selection
		);
	const FOpenMobileHapticPlaybackResult SecondSelection =
		DebounceSubsystem->PlaySemanticFeedback(
			EOpenMobileHapticSemanticEffect::Selection
		);
	TestTrue(TEXT("First selection reaches the backend"),
		FirstSelection.IsAccepted());
	TestEqual(TEXT("Repeated selection is debounced"),
		SecondSelection.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);

	DebounceSubsystem->Deinitialize();
	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsImpactPresetTest,
	"OpenMobile.Haptics.Semantic.ImpactPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsImpactPresetTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("ImpactMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const UFunction* ImpactFunction =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("PlayImpactFeedback")
		);
	TestTrue(TEXT("Impact preset has a Blueprint-callable node"),
		ImpactFunction
			&& ImpactFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));

	struct FExpectedImpact
	{
		EOpenMobileHapticImpactStyle Style;
		EOpenMobileHapticSemanticEffect Effect;
	};
	const FExpectedImpact Styles[] = {
		{EOpenMobileHapticImpactStyle::Light,
			EOpenMobileHapticSemanticEffect::ImpactLight},
		{EOpenMobileHapticImpactStyle::Medium,
			EOpenMobileHapticSemanticEffect::ImpactMedium},
		{EOpenMobileHapticImpactStyle::Heavy,
			EOpenMobileHapticSemanticEffect::ImpactHeavy},
		{EOpenMobileHapticImpactStyle::Soft,
			EOpenMobileHapticSemanticEffect::ImpactSoft},
		{EOpenMobileHapticImpactStyle::Rigid,
			EOpenMobileHapticSemanticEffect::ImpactRigid}
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Styles); ++Index)
	{
		const FName Channel(*FString::Printf(TEXT("Impact%d"), Index));
		const FOpenMobileHapticPlaybackResult Result =
			Subsystem->PlayImpactFeedback(Styles[Index].Style, 0.5f, Channel);
		TestTrue(TEXT("Impact style is accepted"), Result.IsAccepted());
		TestEqual(TEXT("Impact style maps to its stable semantic effect"),
			Backend.LastSemanticRequest.Effect, Styles[Index].Effect);
		TestEqual(TEXT("Normalized impact intensity reaches the backend"),
			Backend.LastSemanticRequest.Intensity, 0.5f);
	}

	const int32 BeforeZeroIntensity = Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult ZeroIntensity =
		Subsystem->PlayImpactFeedback(
			EOpenMobileHapticImpactStyle::Light,
			0.0f,
			TEXT("ZeroImpact")
		);
	TestEqual(TEXT("Zero impact intensity is explicitly suppressed"),
		ZeroIntensity.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Zero impact intensity never reaches the backend"),
		Backend.SemanticSubmissionCount, BeforeZeroIntensity);

	const int32 SubmissionCount = Backend.SemanticSubmissionCount;
	for (const float InvalidIntensity : {
		-0.01f,
		1.01f,
		std::numeric_limits<float>::quiet_NaN()
	})
	{
		const FOpenMobileHapticPlaybackResult Invalid =
			Subsystem->PlayImpactFeedback(
				EOpenMobileHapticImpactStyle::Heavy,
				InvalidIntensity,
				TEXT("InvalidImpact")
			);
		TestEqual(TEXT("Invalid impact intensity is rejected"),
			Invalid.Error.Code, EOpenMobileHapticErrorCode::InvalidRequest);
	}
	const FOpenMobileHapticPlaybackResult InvalidStyle =
		Subsystem->PlayImpactFeedback(
			static_cast<EOpenMobileHapticImpactStyle>(MAX_uint8),
			1.0f,
			TEXT("InvalidImpactStyle")
		);
	TestEqual(TEXT("Unknown impact style is rejected"),
		InvalidStyle.Error.Code, EOpenMobileHapticErrorCode::InvalidRequest);
	TestEqual(TEXT("Invalid impacts never reach the backend"),
		Backend.SemanticSubmissionCount, SubmissionCount);

	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const FOpenMobileHapticPlaybackResult SoftFallback =
		Subsystem->PlayImpactFeedback(
			EOpenMobileHapticImpactStyle::Soft,
			1.0f,
			TEXT("ImpactFallback")
		);
	TestEqual(TEXT("Unavailable impact style uses a predefined fallback"),
		SoftFallback.Outcome, EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Impact fallback order is stable"),
		Backend.LastSemanticResolution.Path,
		EOpenMobileHapticsSemanticPath::PredefinedEffect);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsNotificationPresetTest,
	"OpenMobile.Haptics.Semantic.NotificationPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsNotificationPresetTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("NotificationMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const UFunction* NotificationFunction =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("PlayNotificationFeedback")
		);
	TestTrue(TEXT("Notification preset has a Blueprint-callable node"),
		NotificationFunction
			&& NotificationFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));

	struct FExpectedNotification
	{
		EOpenMobileHapticNotificationType Type;
		EOpenMobileHapticSemanticEffect Effect;
	};
	const FExpectedNotification Notifications[] = {
		{EOpenMobileHapticNotificationType::Success,
			EOpenMobileHapticSemanticEffect::NotificationSuccess},
		{EOpenMobileHapticNotificationType::Warning,
			EOpenMobileHapticSemanticEffect::NotificationWarning},
		{EOpenMobileHapticNotificationType::Error,
			EOpenMobileHapticSemanticEffect::NotificationError}
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Notifications); ++Index)
	{
		const FName Channel(*FString::Printf(TEXT("Notification%d"), Index));
		const FOpenMobileHapticPlaybackResult Result =
			Subsystem->PlayNotificationFeedback(
				Notifications[Index].Type,
				1.0f,
				Channel
			);
		TestTrue(TEXT("Notification preset is accepted"), Result.IsAccepted());
		TestEqual(TEXT("Notification meaning maps to its stable effect"),
			Backend.LastSemanticRequest.Effect, Notifications[Index].Effect);
		TestEqual(TEXT("Notification defaults to the Alerts category"),
			Backend.LastSemanticRequest.Options.Category,
			FName(TEXT("Alerts")));
	}

	FOpenMobileHapticUserPolicy Policy;
	Policy.CategoryScales.Add(TEXT("Alerts"), 0.0f);
	Subsystem->SetUserPolicy(Policy);
	const int32 BeforePolicySuppression = Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult Suppressed =
		Subsystem->PlayNotificationFeedback(
			EOpenMobileHapticNotificationType::Warning
		);
	TestEqual(TEXT("Alerts category can suppress notifications"),
		Suppressed.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Alerts suppression happens before native submission"),
		Backend.SemanticSubmissionCount, BeforePolicySuppression);

	UOpenMobileHapticsSubsystem* RapidSubsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const FOpenMobileHapticPlaybackResult FirstRapid =
		RapidSubsystem->PlayNotificationFeedback(
			EOpenMobileHapticNotificationType::Success
		);
	const FOpenMobileHapticPlaybackResult SecondRapid =
		RapidSubsystem->PlayNotificationFeedback(
			EOpenMobileHapticNotificationType::Error
		);
	TestTrue(TEXT("First rapid notification is accepted"),
		FirstRapid.IsAccepted());
	TestEqual(TEXT("Notification default channel is Alerts"),
		FirstRapid.Channel, FName(TEXT("Alerts")));
	TestEqual(TEXT("Rapid notification repeat is rate limited"),
		SecondRapid.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);

	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const FOpenMobileHapticPlaybackResult BasicFallback =
		RapidSubsystem->PlayNotificationFeedback(
			EOpenMobileHapticNotificationType::Error,
			1.0f,
			TEXT("BasicNotificationFallback")
		);
	TestEqual(TEXT("Notification can use its basic-vibration fallback"),
		BasicFallback.Outcome, EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Notification basic fallback path is reported"),
		Backend.LastSemanticResolution.Path,
		EOpenMobileHapticsSemanticPath::BasicVibration);

	RapidSubsystem->Deinitialize();
	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsGamePresetTest,
	"OpenMobile.Haptics.Semantic.GamePresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsGamePresetTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("GamePresetMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();

	const UFunction* GamePresetFunction =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("PlayGameFeedback")
		);
	TestTrue(TEXT("Game preset has a Blueprint-callable node"),
		GamePresetFunction
			&& GamePresetFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	struct FExpectedPreset
	{
		EOpenMobileHapticGamePreset Preset;
		EOpenMobileHapticSemanticEffect Effect;
		FName Category;
	};
	const FExpectedPreset Presets[] = {
		{EOpenMobileHapticGamePreset::Confirm,
			EOpenMobileHapticSemanticEffect::Confirm, TEXT("UI")},
		{EOpenMobileHapticGamePreset::Reject,
			EOpenMobileHapticSemanticEffect::Reject, TEXT("UI")},
		{EOpenMobileHapticGamePreset::Tick,
			EOpenMobileHapticSemanticEffect::Tick, TEXT("UI")},
		{EOpenMobileHapticGamePreset::Click,
			EOpenMobileHapticSemanticEffect::Click, TEXT("UI")},
		{EOpenMobileHapticGamePreset::Bump,
			EOpenMobileHapticSemanticEffect::Bump, TEXT("Gameplay")},
		{EOpenMobileHapticGamePreset::Damage,
			EOpenMobileHapticSemanticEffect::Damage, TEXT("Gameplay")},
		{EOpenMobileHapticGamePreset::Pickup,
			EOpenMobileHapticSemanticEffect::Pickup, TEXT("Gameplay")},
		{EOpenMobileHapticGamePreset::Achievement,
			EOpenMobileHapticSemanticEffect::Achievement, TEXT("Alerts")}
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Presets); ++Index)
	{
		const FName Channel(*FString::Printf(TEXT("GamePreset%d"), Index));
		const FOpenMobileHapticPlaybackResult Result =
			Subsystem->PlayGameFeedback(Presets[Index].Preset, 1.0f, Channel);
		TestTrue(TEXT("Built-in game preset is accepted"), Result.IsAccepted());
		TestEqual(TEXT("Game preset keeps its stable semantic mapping"),
			Backend.LastSemanticRequest.Effect, Presets[Index].Effect);
		TestEqual(TEXT("Game preset uses its stable category"),
			Backend.LastSemanticRequest.Options.Category,
			Presets[Index].Category);
	}

	FOpenMobileHapticUserPolicy Policy;
	Policy.CategoryScales.Add(TEXT("Gameplay"), 0.0f);
	Subsystem->SetUserPolicy(Policy);
	const int32 BeforeCategorySuppression = Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult CategorySuppressed =
		Subsystem->PlayGameFeedback(
			EOpenMobileHapticGamePreset::Damage,
			1.0f,
			TEXT("ScaledDamage")
		);
	TestEqual(TEXT("Game preset category scaling is applied"),
		CategorySuppressed.Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Suppressed game preset never reaches the backend"),
		Backend.SemanticSubmissionCount, BeforeCategorySuppression);
	Subsystem->SetUserPolicy({});

	UOpenMobileHapticLibrary* Library =
		NewObject<UOpenMobileHapticLibrary>(
			GetTransientPackage(),
			TEXT("GamePresetTestLibrary")
		);
	Library->GamePresetOverrides = {
		{EOpenMobileHapticGamePreset::Confirm, TEXT("CustomConfirm")},
		{EOpenMobileHapticGamePreset::Reject, TEXT("CustomReject")}
	};
	FOpenMobileHapticNamedLibrarySettings LibrarySettings;
	LibrarySettings.Name = TEXT("GamePresets");
	LibrarySettings.Asset = FSoftObjectPath(Library);
	Settings->NamedLibraries = {LibrarySettings};
	const FOpenMobileHapticPlaybackResult Override =
		Subsystem->PlayGameFeedback(
			EOpenMobileHapticGamePreset::Confirm,
			1.0f,
			TEXT("GameOverride")
		);
	TestTrue(TEXT("Loaded named-library override is accepted"),
		Override.IsAccepted());
	TestEqual(TEXT("Named-library override reaches named playback"),
		Backend.LastNamedRequest.PatternName, FName(TEXT("CustomConfirm")));

	Backend.bFailNamedSubmissions = true;
	const FOpenMobileHapticPlaybackResult UnsupportedOverride =
		Subsystem->PlayGameFeedback(
			EOpenMobileHapticGamePreset::Reject,
			1.0f,
			TEXT("UnsupportedGameOverride")
		);
	TestEqual(TEXT("Unsupported override uses the built-in fallback"),
		UnsupportedOverride.Outcome,
		EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Built-in fallback preserves the preset meaning"),
		Backend.LastSemanticRequest.Effect,
		EOpenMobileHapticSemanticEffect::Reject);
	Backend.bFailNamedSubmissions = false;

	LibrarySettings.Asset =
		FSoftObjectPath(TEXT("/Game/Haptics/Missing.Missing"));
	Settings->NamedLibraries = {LibrarySettings};
	const int32 NamedBeforeMissingAsset = Backend.NamedSubmissionCount;
	const FOpenMobileHapticPlaybackResult MissingAsset =
		Subsystem->PlayGameFeedback(
			EOpenMobileHapticGamePreset::Tick,
			1.0f,
			TEXT("MissingGameOverride")
		);
	TestTrue(TEXT("Missing override asset keeps the built-in preset"),
		MissingAsset.IsAccepted());
	TestEqual(TEXT("Missing override asset is not synchronously loaded"),
		Backend.NamedSubmissionCount, NamedBeforeMissingAsset);
	TestEqual(TEXT("Missing override keeps the stable semantic mapping"),
		Backend.LastSemanticRequest.Effect,
		EOpenMobileHapticSemanticEffect::Tick);

	Settings->NamedLibraries = SavedLibraries;
	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsSemanticRateLimitTest,
	"OpenMobile.Haptics.Semantic.RateLimitBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSemanticRateLimitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsRateLimiter SelectionLimiter;
	TestFalse(TEXT("First selection is allowed"),
		SelectionLimiter.ShouldSuppress(TEXT("UI"), true, 10.0,
			0.0, 0.04, 30));
	TestTrue(TEXT("Selection inside the debounce window is suppressed"),
		SelectionLimiter.ShouldSuppress(TEXT("UI"), true, 10.039,
			0.0, 0.04, 30));
	TestFalse(TEXT("Selection at the debounce boundary is allowed"),
		SelectionLimiter.ShouldSuppress(TEXT("UI"), true, 10.04,
			0.0, 0.04, 30));

	FOpenMobileHapticsRateLimiter ChannelLimiter;
	TestFalse(TEXT("First channel submission is allowed"),
		ChannelLimiter.ShouldSuppress(TEXT("Gameplay"), false, 20.0,
			0.02, 0.04, 30));
	TestTrue(TEXT("Channel submission inside its interval is suppressed"),
		ChannelLimiter.ShouldSuppress(TEXT("Gameplay"), false, 20.019,
			0.02, 0.04, 30));
	TestFalse(TEXT("Channel submission at its interval is allowed"),
		ChannelLimiter.ShouldSuppress(TEXT("Gameplay"), false, 20.02,
			0.02, 0.04, 30));

	FOpenMobileHapticsRateLimiter BurstLimiter;
	TestFalse(TEXT("First burst event is allowed"),
		BurstLimiter.ShouldSuppress(TEXT("A"), false, 30.0,
			0.0, 0.0, 2));
	TestFalse(TEXT("Second burst event is allowed"),
		BurstLimiter.ShouldSuppress(TEXT("B"), false, 30.1,
			0.0, 0.0, 2));
	TestTrue(TEXT("Submission cap suppresses the next event"),
		BurstLimiter.ShouldSuppress(TEXT("C"), false, 30.2,
			0.0, 0.0, 2));
	TestFalse(TEXT("Old burst events expire at one second"),
		BurstLimiter.ShouldSuppress(TEXT("D"), false, 31.0,
			0.0, 0.0, 2));

	FOpenMobileHapticsRateLimiter PerChannelLimiter;
	TestFalse(TEXT("Fast selection channel accepts its first event"),
		PerChannelLimiter.ShouldSuppress(TEXT("Fast"), true, 40.0,
			0.02, 0.02, 30));
	TestFalse(TEXT("Fast selection channel uses its shorter interval"),
		PerChannelLimiter.ShouldSuppress(TEXT("Fast"), true, 40.025,
			0.02, 0.02, 30));
	TestFalse(TEXT("Slow selection channel accepts its first event"),
		PerChannelLimiter.ShouldSuppress(TEXT("Slow"), true, 40.0,
			0.04, 0.02, 30));
	TestTrue(TEXT("Slow selection channel keeps its longer interval"),
		PerChannelLimiter.ShouldSuppress(TEXT("Slow"), true, 40.025,
			0.04, 0.02, 30));
	return true;
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
	Low.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Low.PreparationState =
		EOpenMobileHapticsBackendPreparationState::Preparing;
	FMockBackend High(TEXT("High"), 10);
	High.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	High.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
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
	Subsystem->PlaySemanticFeedback(
		EOpenMobileHapticSemanticEffect::Click,
		1.0f,
		TEXT("Replacement")
	);
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

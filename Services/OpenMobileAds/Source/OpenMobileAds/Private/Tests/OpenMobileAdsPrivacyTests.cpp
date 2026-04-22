#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileAdsCanRequestPolicy.h"
#include "OpenMobileAdsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OpenMobileAdsPrivacyTests
{
	class FMockConsentProvider
	{
	public:
		explicit FMockConsentProvider(UOpenMobileAdsSubsystem& InSubsystem)
			: Subsystem(InSubsystem)
		{
		}

		void BeginRefresh()
		{
			Subsystem.ApplyConsentStatusUpdate(
				FOpenMobileAdsConsentStatusUpdate::BeginRefresh(Source)
			);
		}

		void BeginFormPresentation()
		{
			Subsystem.ApplyConsentStatusUpdate(
				FOpenMobileAdsConsentStatusUpdate::BeginFormPresentation(Source)
			);
		}

		void BeginReset()
		{
			Subsystem.ApplyConsentStatusUpdate(
				FOpenMobileAdsConsentStatusUpdate::BeginReset(Source)
			);
		}

		void Complete(
			EOpenMobileAdsConsentStatus Status,
			FOpenMobileAdsConsentProviderDetails ProviderDetails = {}
		)
		{
			Subsystem.ApplyConsentStatusUpdate(
				FOpenMobileAdsConsentStatusUpdate::Complete(
					Status,
					Source,
					MoveTemp(ProviderDetails)
				)
			);
		}

		void Fail()
		{
			Subsystem.ApplyConsentStatusUpdate(
				FOpenMobileAdsConsentStatusUpdate::Fail(
					Source,
					FOpenMobileAdsError::Make(
						EOpenMobileAdsErrorCode::NativeFailure,
						EOpenMobileAdsFailureStage::None,
						NAME_None,
						TEXT("The mock consent operation failed.")
					)
				)
			);
		}

		FName Source = TEXT("MockConsent");

	private:
		UOpenMobileAdsSubsystem& Subsystem;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsConsentStatusTransitionTest,
	"OpenMobile.Ads.Privacy.ConsentStatus.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsConsentStatusTransitionTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsPrivacyTests;
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	FMockConsentProvider Provider(*Subsystem);

	TestNotNull(
		TEXT("Blueprints can poll the consent status snapshot"),
		UOpenMobileAdsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UOpenMobileAdsSubsystem, GetConsentStatus)
		)
	);
	TestNotNull(
		TEXT("Blueprints can bind the consent status event"),
		UOpenMobileAdsSubsystem::StaticClass()->FindPropertyByName(
			GET_MEMBER_NAME_CHECKED(UOpenMobileAdsSubsystem, OnConsentStatusChanged)
		)
	);

	TArray<FOpenMobileAdsPrivacySnapshot> Events;
	const FDelegateHandle EventHandle =
		Subsystem->OnNativeConsentStatusChanged().AddLambda(
			[&Events](const FOpenMobileAdsPrivacySnapshot& Snapshot)
			{
				Events.Add(Snapshot);
			}
		);

	FOpenMobileAdsConsentProviderDetails RequiredDetails;
	RequiredDetails.bIsAvailable = true;
	RequiredDetails.RawStatus = TEXT("REQUIRED");
	RequiredDetails.RawMessage = TEXT("mock form required");
	Provider.Complete(
		EOpenMobileAdsConsentStatus::Required,
		MoveTemp(RequiredDetails)
	);
	FOpenMobileAdsPrivacySnapshot Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("Required is normalized"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Required);
	TestEqual(TEXT("Completed status is idle"), Snapshot.ConsentActivity, EOpenMobileAdsConsentActivity::Idle);
	TestEqual(TEXT("The source is exposed"), Snapshot.Source, Provider.Source);
	TestTrue(TEXT("The last update is recorded"), Snapshot.LastUpdated != FDateTime());
	TestTrue(TEXT("Raw provider details are optional and present"), Snapshot.ProviderDetails.bIsAvailable);
	TestEqual(TEXT("The raw status is preserved"), Snapshot.ProviderDetails.RawStatus, FString(TEXT("REQUIRED")));
	TestFalse(TEXT("Consent status does not bypass initialization"), Snapshot.bCanRequestAds);

	Provider.BeginFormPresentation();
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("Form presentation preserves Required"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Required);
	TestEqual(
		TEXT("Form presentation is observable"),
		Snapshot.ConsentActivity,
		EOpenMobileAdsConsentActivity::PresentingForm
	);
	Provider.Fail();
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("A form failure preserves Required"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Required);
	TestEqual(TEXT("A form failure returns to idle"), Snapshot.ConsentActivity, EOpenMobileAdsConsentActivity::Idle);
	TestEqual(TEXT("Consent failures use the consent stage"), Snapshot.Error.Stage, EOpenMobileAdsFailureStage::Consent);
	TestEqual(TEXT("Consent failures retain their source"), Snapshot.Error.Provider, Provider.Source);

	Provider.BeginFormPresentation();
	Provider.Complete(EOpenMobileAdsConsentStatus::Granted);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("Granted is normalized"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Granted);
	TestFalse(TEXT("Granted still requires initialized ads"), Snapshot.bCanRequestAds);
	TestFalse(TEXT("A completed status clears stale raw details"), Snapshot.ProviderDetails.bIsAvailable);
	TestFalse(TEXT("A completed status clears the prior failure"), Snapshot.Error.IsSet());

	Provider.BeginRefresh();
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("Refresh preserves the last status"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Granted);
	TestEqual(TEXT("Refresh activity is observable"), Snapshot.ConsentActivity, EOpenMobileAdsConsentActivity::Refreshing);
	Provider.Complete(EOpenMobileAdsConsentStatus::Denied);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("Denied is normalized"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Denied);
	TestFalse(TEXT("Denied does not allow requests"), Snapshot.bCanRequestAds);

	Provider.BeginRefresh();
	Provider.Complete(EOpenMobileAdsConsentStatus::NotRequired);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("NotRequired is normalized"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::NotRequired);
	TestFalse(TEXT("NotRequired still requires initialized ads"), Snapshot.bCanRequestAds);
	Provider.BeginRefresh();
	Provider.Fail();
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(
		TEXT("A refresh failure preserves the last status"),
		Snapshot.ConsentStatus,
		EOpenMobileAdsConsentStatus::NotRequired
	);
	TestEqual(TEXT("A refresh failure returns to idle"), Snapshot.ConsentActivity, EOpenMobileAdsConsentActivity::Idle);

	Provider.BeginReset();
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(
		TEXT("Reset immediately returns consent to Unknown"),
		Snapshot.ConsentStatus,
		EOpenMobileAdsConsentStatus::Unknown
	);
	TestEqual(TEXT("Reset activity is observable"), Snapshot.ConsentActivity, EOpenMobileAdsConsentActivity::Resetting);
	TestFalse(TEXT("Reset does not allow requests"), Snapshot.bCanRequestAds);
	TestFalse(TEXT("Reset clears raw provider details"), Snapshot.ProviderDetails.bIsAvailable);
	Provider.Fail();
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("A reset failure remains Unknown"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Unknown);
	TestEqual(TEXT("A reset failure returns to idle"), Snapshot.ConsentActivity, EOpenMobileAdsConsentActivity::Idle);
	Provider.Complete(EOpenMobileAdsConsentStatus::Unknown);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("Unknown is normalized"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Unknown);
	TestFalse(TEXT("A completed Unknown status clears the reset failure"), Snapshot.Error.IsSet());

	TestEqual(TEXT("Every accepted transition broadcasts once"), Events.Num(), 14);
	TestEqual(TEXT("The final native event matches polling"), Events.Last().ConsentStatus, Snapshot.ConsentStatus);
	Subsystem->OnNativeConsentStatusChanged().Remove(EventHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCanRequestPolicyTest,
	"OpenMobile.Ads.Privacy.CanRequestAds.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCanRequestPolicyTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdsCanRequestAdsContext Context;
	Context.ServiceState = EOpenMobileAdsServiceState::Ready;
	Context.bProviderAvailable = true;
	Context.Provider = TEXT("MockAds");
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Context.ConsentActivity = EOpenMobileAdsConsentActivity::Idle;
	Context.bConsentStatusFresh = true;
	Context.ProviderPolicy.State =
		EOpenMobileAdsProviderRequestPolicyState::Allowed;

	auto ExpectBlock = [this, &Context](
		EOpenMobileAdsCanRequestAdsBlockReason ExpectedReason,
		EOpenMobileAdsCanRequestAdsBlockType ExpectedType,
		const TCHAR* Message
	)
	{
		const FOpenMobileAdsCanRequestAdsResult Result =
			FOpenMobileAdsCanRequestPolicy::Evaluate(Context);
		TestFalse(Message, Result.bCanRequestAds);
		TestEqual(TEXT("The blocking reason is structured"), Result.BlockReason, ExpectedReason);
		TestEqual(TEXT("The blocking type is structured"), Result.BlockType, ExpectedType);
		TestFalse(TEXT("A blocked decision explains itself"), Result.Explanation.IsEmpty());
	};

	const FOpenMobileAdsCanRequestAdsResult Allowed =
		FOpenMobileAdsCanRequestPolicy::Evaluate(Context);
	TestTrue(TEXT("A ready, fresh, allowed request passes"), Allowed.bCanRequestAds);
	TestEqual(TEXT("An allowed request has no reason"), Allowed.BlockReason, EOpenMobileAdsCanRequestAdsBlockReason::None);
	TestEqual(TEXT("An allowed request has no block type"), Allowed.BlockType, EOpenMobileAdsCanRequestAdsBlockType::None);

	Context.bProviderAvailable = false;
	Context.ConsentActivity = EOpenMobileAdsConsentActivity::Resetting;
	Context.bConsentStatusFresh = false;
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::Denied;
	Context.ProviderPolicy.State =
		EOpenMobileAdsProviderRequestPolicyState::Blocked;
	Context.ServiceState = EOpenMobileAdsServiceState::Uninitialized;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::InitializationNotStarted,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Uninitialized service is blocked")
	);
	Context.ServiceState = EOpenMobileAdsServiceState::Initializing;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::InitializationPending,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Initializing service is blocked")
	);
	Context.ServiceState = EOpenMobileAdsServiceState::Failed;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::InitializationFailed,
		EOpenMobileAdsCanRequestAdsBlockType::Terminal,
		TEXT("Failed initialization is blocked")
	);
	Context.ServiceState = EOpenMobileAdsServiceState::ShuttingDown;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ShuttingDown,
		EOpenMobileAdsCanRequestAdsBlockType::Terminal,
		TEXT("Shutdown is blocked")
	);

	Context.ServiceState = EOpenMobileAdsServiceState::Ready;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ProviderUnavailable,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Missing initialized provider is blocked")
	);

	Context.bProviderAvailable = true;
	Context.ConsentActivity = EOpenMobileAdsConsentActivity::Resetting;
	Context.bConsentStatusFresh = false;
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::Unknown;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentResetting,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Reset takes precedence over other consent blockers")
	);
	Context.ConsentActivity = EOpenMobileAdsConsentActivity::PresentingForm;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentFormPresenting,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Form presentation takes precedence over freshness")
	);
	Context.ConsentActivity = EOpenMobileAdsConsentActivity::Refreshing;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentRefreshing,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("A stale refresh is reported")
	);
	Context.ConsentActivity = EOpenMobileAdsConsentActivity::Idle;
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::Denied;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentStale,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Stale consent takes precedence over its old status")
	);

	Context.bConsentStatusFresh = true;
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::Unknown;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentUnknown,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Unknown consent is temporary")
	);
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::Required;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentRequired,
		EOpenMobileAdsCanRequestAdsBlockType::UserDecision,
		TEXT("Required consent needs a user decision")
	);
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::Denied;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentDenied,
		EOpenMobileAdsCanRequestAdsBlockType::UserDecision,
		TEXT("Denied consent remains a user-decision block")
	);

	Context.ProviderPolicy.State =
		EOpenMobileAdsProviderRequestPolicyState::Allowed;
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::NotRequired;
	Context.ConsentActivity = EOpenMobileAdsConsentActivity::Refreshing;
	TestTrue(
		TEXT("A refresh preserves a fresh allowed decision"),
		FOpenMobileAdsCanRequestPolicy::Evaluate(Context).bCanRequestAds
	);
	Context.ConsentActivity = EOpenMobileAdsConsentActivity::Idle;
	Context.ProviderPolicy.State =
		EOpenMobileAdsProviderRequestPolicyState::TemporarilyBlocked;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ProviderPolicy,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("A temporary provider policy is classified")
	);
	Context.ProviderPolicy.State =
		EOpenMobileAdsProviderRequestPolicyState::UserDecisionRequired;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ProviderPolicy,
		EOpenMobileAdsCanRequestAdsBlockType::UserDecision,
		TEXT("A provider user decision is classified")
	);
	Context.ProviderPolicy.State =
		EOpenMobileAdsProviderRequestPolicyState::Blocked;
	Context.ProviderPolicy.Explanation = TEXT("The mock provider policy blocks requests.");
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ProviderPolicy,
		EOpenMobileAdsCanRequestAdsBlockType::Configuration,
		TEXT("A terminal provider policy is classified")
	);
	return true;
}

#endif

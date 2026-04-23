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
	TestEqual(
		TEXT("Granted retains its required consent requirement"),
		Snapshot.ConsentRequirement,
		EOpenMobileAdsConsentRequirement::Required
	);
	TestEqual(
		TEXT("Granted retains legacy request eligibility"),
		Snapshot.ConsentRequestState,
		EOpenMobileAdsConsentRequestState::Allowed
	);
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
	TestEqual(
		TEXT("Denied remains a required consent decision"),
		Snapshot.ConsentRequirement,
		EOpenMobileAdsConsentRequirement::Required
	);
	TestEqual(
		TEXT("Denied blocks legacy request eligibility"),
		Snapshot.ConsentRequestState,
		EOpenMobileAdsConsentRequestState::Blocked
	);
	TestFalse(TEXT("Denied does not allow requests"), Snapshot.bCanRequestAds);

	Provider.BeginRefresh();
	Provider.Complete(EOpenMobileAdsConsentStatus::NotRequired);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("NotRequired is normalized"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::NotRequired);
	TestEqual(
		TEXT("NotRequired has an explicit consent requirement"),
		Snapshot.ConsentRequirement,
		EOpenMobileAdsConsentRequirement::NotRequired
	);
	TestEqual(
		TEXT("NotRequired allows legacy request eligibility"),
		Snapshot.ConsentRequestState,
		EOpenMobileAdsConsentRequestState::Allowed
	);
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
	FOpenMobileAdsGdprStateContractTest,
	"OpenMobile.Ads.Privacy.Gdpr.StateAndFreshness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsGdprStateContractTest::RunTest(const FString& Parameters)
{
	const FOpenMobileAdsConsentStatusUpdate GenericObtained =
		FOpenMobileAdsConsentStatusUpdate::Complete(
			EOpenMobileAdsConsentStatus::Obtained,
			TEXT("GenericConsent")
		);
	TestEqual(
		TEXT("A generic obtained status does not imply request eligibility"),
		GenericObtained.RequestState,
		EOpenMobileAdsConsentRequestState::Unknown
	);

	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	const FDateTime Now(2026, 8, 21, 12, 0, 0);
	const FDateTime ExpiresAt = Now + FTimespan::FromHours(1.0);
	Subsystem->ApplyConsentStatusUpdate(
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::Obtained,
			EOpenMobileAdsGdprApplicability::Applicable,
			EOpenMobileAdsConsentRequirement::Required,
			EOpenMobileAdsConsentRequestState::Allowed,
			TEXT("MockGdpr"),
			{},
			true,
			ExpiresAt,
			true
		)
	);

	FOpenMobileAdsPrivacySnapshot Snapshot = Subsystem->GetConsentStatus();
	TestEqual(
		TEXT("Obtained remains distinct from granted consent"),
		Snapshot.ConsentStatus,
		EOpenMobileAdsConsentStatus::Obtained
	);
	TestEqual(
		TEXT("GDPR applicability is explicit"),
		Snapshot.GdprApplicability,
		EOpenMobileAdsGdprApplicability::Applicable
	);
	TestEqual(
		TEXT("Consent requirement is separate from the decision"),
		Snapshot.ConsentRequirement,
		EOpenMobileAdsConsentRequirement::Required
	);
	TestEqual(
		TEXT("Provider ad-request eligibility is explicit"),
		Snapshot.ConsentRequestState,
		EOpenMobileAdsConsentRequestState::Allowed
	);
	TestFalse(TEXT("A restored provider result starts stale"), Snapshot.bConsentStatusFresh);
	TestEqual(TEXT("Provider expiry is retained"), Snapshot.ConsentExpiresAt, ExpiresAt);
	TestTrue(
		TEXT("Provider-owned persisted state is identified"),
		Snapshot.bRestoredFromProviderStorage
	);

	Subsystem->ApplyConsentStatusUpdate(
		FOpenMobileAdsConsentStatusUpdate::BeginRefresh(TEXT("MockGdpr"))
	);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(
		TEXT("Refreshing preserves the restored decision"),
		Snapshot.ConsentStatus,
		EOpenMobileAdsConsentStatus::Obtained
	);
	TestEqual(
		TEXT("Refreshing preserves provider expiry"),
		Snapshot.ConsentExpiresAt,
		ExpiresAt
	);

	Subsystem->ApplyConsentStatusUpdate(
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::Obtained,
			EOpenMobileAdsGdprApplicability::Applicable,
			EOpenMobileAdsConsentRequirement::Required,
			EOpenMobileAdsConsentRequestState::Allowed,
			TEXT("MockGdpr"),
			{},
			true,
			ExpiresAt
		)
	);
	Snapshot = Subsystem->GetConsentStatus();
	TestTrue(TEXT("A completed session refresh is fresh"), Snapshot.bConsentStatusFresh);
	TestFalse(
		TEXT("A refreshed result is no longer marked restored"),
		Snapshot.bRestoredFromProviderStorage
	);
	TestTrue(
		TEXT("Consent is fresh immediately before its provider expiry"),
		Snapshot.IsConsentStatusFreshAt(ExpiresAt - FTimespan(1))
	);
	TestFalse(
		TEXT("Consent expires at the exact provider deadline"),
		Snapshot.IsConsentStatusFreshAt(ExpiresAt)
	);
	TestFalse(
		TEXT("Consent stays expired after its provider deadline"),
		Snapshot.IsConsentStatusFreshAt(ExpiresAt + FTimespan(1))
	);

	Subsystem->ApplyConsentStatusUpdate(
		FOpenMobileAdsConsentStatusUpdate::Fail(
			TEXT("MockGdpr"),
			FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("The GDPR refresh failed.")
			)
		)
	);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(
		TEXT("A refresh error preserves the last provider decision"),
		Snapshot.ConsentStatus,
		EOpenMobileAdsConsentStatus::Obtained
	);
	TestEqual(
		TEXT("A refresh error preserves the provider expiry"),
		Snapshot.ConsentExpiresAt,
		ExpiresAt
	);
	TestTrue(TEXT("A refresh error remains observable"), Snapshot.Error.IsSet());
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

	Context.ProviderPolicy = FOpenMobileAdsProviderRequestPolicy();
	Context.ConsentStatus = EOpenMobileAdsConsentStatus::Obtained;
	Context.ConsentRequestState = EOpenMobileAdsConsentRequestState::Unknown;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentProviderBlocked,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Obtained consent does not imply ad-request eligibility")
	);
	Context.ConsentRequestState = EOpenMobileAdsConsentRequestState::Allowed;
	TestTrue(
		TEXT("Provider-approved obtained consent allows requests"),
		FOpenMobileAdsCanRequestPolicy::Evaluate(Context).bCanRequestAds
	);
	Context.ConsentRequestState = EOpenMobileAdsConsentRequestState::Blocked;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentProviderBlocked,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary,
		TEXT("Provider-blocked obtained consent remains blocked")
	);

	Context.ConsentStatus = EOpenMobileAdsConsentStatus::NotRequired;
	Context.ConsentRequestState = EOpenMobileAdsConsentRequestState::Allowed;
	Context.UsPrivacy.Applicability =
		EOpenMobileAdsUsPrivacyApplicability::Applicable;
	Context.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedOut;
	Context.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Standard;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::PrivacySignalInvalid,
		EOpenMobileAdsCanRequestAdsBlockType::Configuration,
		TEXT("An opt-out cannot use standard data processing")
	);
	Context.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Unspecified;
	ExpectBlock(
		EOpenMobileAdsCanRequestAdsBlockReason::PrivacySignalInvalid,
		EOpenMobileAdsCanRequestAdsBlockType::Configuration,
		TEXT("An opt-out requires an explicit provider signal")
	);
	Context.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::ProviderManaged;
	TestTrue(
		TEXT("A provider-managed opt-out can request ads"),
		FOpenMobileAdsCanRequestPolicy::Evaluate(Context).bCanRequestAds
	);
	Context.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Restricted;
	TestTrue(
		TEXT("A restricted opt-out can request ads"),
		FOpenMobileAdsCanRequestPolicy::Evaluate(Context).bCanRequestAds
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsUsPrivacyStateContractTest,
	"OpenMobile.Ads.Privacy.UsState.StateAndChoiceChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsUsPrivacyStateContractTest::RunTest(
	const FString& Parameters
)
{
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	FOpenMobileAdsPrivacySnapshot Snapshot = Subsystem->GetPrivacySnapshot();
	TestEqual(
		TEXT("US-state applicability starts unknown"),
		Snapshot.UsPrivacy.Applicability,
		EOpenMobileAdsUsPrivacyApplicability::Unknown
	);
	TestEqual(
		TEXT("The US-state choice starts unknown"),
		Snapshot.UsPrivacy.Choice,
		EOpenMobileAdsUsPrivacyChoice::Unknown
	);
	TestEqual(
		TEXT("Privacy-options requirements start unknown"),
		Snapshot.UsPrivacy.PrivacyOptionsRequirement,
		EOpenMobileAdsPrivacyOptionsRequirement::Unknown
	);
	TestFalse(
		TEXT("Privacy-options availability starts false"),
		Snapshot.UsPrivacy.bPrivacyOptionsFormAvailable
	);

	TArray<FOpenMobileAdsUsPrivacyState> Events;
	const FDelegateHandle EventHandle =
		Subsystem->OnNativeConsentStatusChanged().AddLambda(
			[&Events](const FOpenMobileAdsPrivacySnapshot& Event)
			{
				Events.Add(Event.UsPrivacy);
			}
		);
	auto ApplyState = [Subsystem](FOpenMobileAdsUsPrivacyState UsPrivacy)
	{
		FOpenMobileAdsConsentStatusUpdate Update =
			FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
				EOpenMobileAdsConsentStatus::NotRequired,
				EOpenMobileAdsGdprApplicability::Unknown,
				EOpenMobileAdsConsentRequirement::NotRequired,
				EOpenMobileAdsConsentRequestState::Allowed,
				TEXT("MockUsPrivacy")
			);
		Update.UsPrivacy = UsPrivacy;
		Subsystem->ApplyConsentStatusUpdate(MoveTemp(Update));
	};

	FOpenMobileAdsUsPrivacyState State;
	State.Applicability = EOpenMobileAdsUsPrivacyApplicability::NotApplicable;
	State.PrivacyOptionsRequirement =
		EOpenMobileAdsPrivacyOptionsRequirement::NotRequired;
	State.DataProcessingMode = EOpenMobileAdsDataProcessingMode::Standard;
	ApplyState(State);
	Snapshot = Subsystem->GetPrivacySnapshot();
	TestEqual(
		TEXT("A not-applicable provider result is represented explicitly"),
		Snapshot.UsPrivacy.Applicability,
		EOpenMobileAdsUsPrivacyApplicability::NotApplicable
	);

	State.Applicability = EOpenMobileAdsUsPrivacyApplicability::Applicable;
	State.Choice = EOpenMobileAdsUsPrivacyChoice::OptedIn;
	State.PrivacyOptionsRequirement =
		EOpenMobileAdsPrivacyOptionsRequirement::Required;
	State.bPrivacyOptionsFormAvailable = true;
	ApplyState(State);
	Snapshot = Subsystem->GetPrivacySnapshot();
	TestEqual(
		TEXT("An opted-in choice is normalized"),
		Snapshot.UsPrivacy.Choice,
		EOpenMobileAdsUsPrivacyChoice::OptedIn
	);
	TestEqual(
		TEXT("A required privacy-options path is exposed"),
		Snapshot.UsPrivacy.PrivacyOptionsRequirement,
		EOpenMobileAdsPrivacyOptionsRequirement::Required
	);
	TestTrue(
		TEXT("An available privacy-options form is exposed"),
		Snapshot.UsPrivacy.bPrivacyOptionsFormAvailable
	);

	State.Choice = EOpenMobileAdsUsPrivacyChoice::OptedOut;
	State.DataProcessingMode = EOpenMobileAdsDataProcessingMode::Restricted;
	ApplyState(State);
	Snapshot = Subsystem->GetPrivacySnapshot();
	TestEqual(
		TEXT("A changed opt-out choice replaces the prior choice"),
		Snapshot.UsPrivacy.Choice,
		EOpenMobileAdsUsPrivacyChoice::OptedOut
	);
	TestEqual(
		TEXT("The changed choice carries its provider request signal"),
		Snapshot.UsPrivacy.DataProcessingMode,
		EOpenMobileAdsDataProcessingMode::Restricted
	);
	TestEqual(TEXT("Each completed choice broadcasts once"), Events.Num(), 3);

	Subsystem->OnNativeConsentStatusChanged().Remove(EventHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsConsentSignalsContractTest,
	"OpenMobile.Ads.Privacy.ConsentSignals.Normalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsConsentSignalsContractTest::RunTest(
	const FString& Parameters
)
{
	const int32 GdprSignal = static_cast<int32>(
		EOpenMobileAdsConsentSignal::Gdpr
	);
	const int32 UsPrivacySignal = static_cast<int32>(
		EOpenMobileAdsConsentSignal::UsPrivacy
	);
	const int32 ChildDirectedSignal = static_cast<int32>(
		EOpenMobileAdsConsentSignal::ChildDirected
	);
	const int32 UnderAgeSignal = static_cast<int32>(
		EOpenMobileAdsConsentSignal::UnderAgeOfConsent
	);

	FOpenMobileAdsConsentSignals Signals;
	TestEqual(TEXT("Unknown signals are not configured"), Signals.GetConfiguredSignalMask(), 0);
	TestEqual(TEXT("Unknown signals are not required"), Signals.GetRequiredSignalMask(), 0);

	Signals.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::No;
	Signals.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::Yes;
	TestEqual(
		TEXT("Explicit age signals are configured and required"),
		Signals.GetRequiredSignalMask(),
		ChildDirectedSignal | UnderAgeSignal
	);

	Signals.bConsentStatusFresh = true;
	Signals.ConsentStatus = EOpenMobileAdsConsentStatus::NotRequired;
	Signals.GdprApplicability =
		EOpenMobileAdsGdprApplicability::NotApplicable;
	Signals.ConsentRequirement =
		EOpenMobileAdsConsentRequirement::NotRequired;
	Signals.ConsentRequestState = EOpenMobileAdsConsentRequestState::Allowed;
	TestTrue(
		TEXT("A fresh not-applicable GDPR result is configured"),
		(Signals.GetConfiguredSignalMask() & GdprSignal) != 0
	);
	TestFalse(
		TEXT("A not-applicable GDPR result is not required"),
		(Signals.GetRequiredSignalMask() & GdprSignal) != 0
	);

	Signals.GdprApplicability = EOpenMobileAdsGdprApplicability::Applicable;
	Signals.ConsentStatus = EOpenMobileAdsConsentStatus::Obtained;
	Signals.ConsentRequirement = EOpenMobileAdsConsentRequirement::Required;
	Signals.UsPrivacy.Applicability =
		EOpenMobileAdsUsPrivacyApplicability::Applicable;
	Signals.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedIn;
	Signals.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Standard;
	TestEqual(
		TEXT("Applicable consent state requires every configured signal"),
		Signals.GetRequiredSignalMask(),
		FOpenMobileAdsConsentSignals::AllSignalMask
	);

	FOpenMobileAdsConsentSignals Changed = Signals;
	Changed.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedOut;
	Changed.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Restricted;
	TestEqual(
		TEXT("A changed US choice produces one propagation delta"),
		Changed.GetChangedSignalMask(Signals),
		UsPrivacySignal
	);
	Changed.bConsentStatusFresh = false;
	TestEqual(
		TEXT("Consent freshness changes both regional signals"),
		Changed.GetChangedSignalMask(Signals),
		GdprSignal | UsPrivacySignal
	);
	TestEqual(
		TEXT("Stale regional state leaves only explicit age signals required"),
		Changed.GetRequiredSignalMask(),
		ChildDirectedSignal | UnderAgeSignal
	);
	return true;
}

#endif

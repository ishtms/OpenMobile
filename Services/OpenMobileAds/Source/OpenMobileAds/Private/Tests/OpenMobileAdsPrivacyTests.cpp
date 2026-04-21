#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
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
	TestFalse(TEXT("Required consent blocks the existing privacy gate"), Snapshot.bCanRequestAds);

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
	TestTrue(TEXT("Granted allows the existing privacy gate"), Snapshot.bCanRequestAds);
	TestFalse(TEXT("A completed status clears stale raw details"), Snapshot.ProviderDetails.bIsAvailable);
	TestFalse(TEXT("A completed status clears the prior failure"), Snapshot.Error.IsSet());

	Provider.BeginRefresh();
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("Refresh preserves the last status"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Granted);
	TestEqual(TEXT("Refresh activity is observable"), Snapshot.ConsentActivity, EOpenMobileAdsConsentActivity::Refreshing);
	Provider.Complete(EOpenMobileAdsConsentStatus::Denied);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("Denied is normalized"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::Denied);
	TestFalse(TEXT("Denied blocks the existing privacy gate"), Snapshot.bCanRequestAds);

	Provider.BeginRefresh();
	Provider.Complete(EOpenMobileAdsConsentStatus::NotRequired);
	Snapshot = Subsystem->GetConsentStatus();
	TestEqual(TEXT("NotRequired is normalized"), Snapshot.ConsentStatus, EOpenMobileAdsConsentStatus::NotRequired);
	TestTrue(TEXT("NotRequired allows the existing privacy gate"), Snapshot.bCanRequestAds);
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
	TestFalse(TEXT("Reset blocks the existing privacy gate"), Snapshot.bCanRequestAds);
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

#endif

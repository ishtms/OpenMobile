#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsDiagnostics.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsPrivacy.h"
#include "OpenMobileAdsResults.h"
#include "OpenMobileAdsRevenue.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPublicContractsTest,
	"OpenMobile.Ads.Contracts.PublicTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPublicContractsTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdFormatCapabilities Rewarded;
	Rewarded.Format = EOpenMobileAdFormat::Rewarded;
	Rewarded.bCanLoad = true;
	Rewarded.bCanShow = true;
	Rewarded.bReportsReward = true;

	FOpenMobileAdsProviderCapabilities Capabilities;
	Capabilities.Provider = TEXT("MockAds");
	Capabilities.Formats.Add(Rewarded);

	TestTrue(TEXT("Rewarded is supported"), Capabilities.SupportsFormat(EOpenMobileAdFormat::Rewarded));
	TestFalse(TEXT("Interstitial is unsupported"), Capabilities.SupportsFormat(EOpenMobileAdFormat::Interstitial));
	TestTrue(TEXT("Reward support is explicit"), Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded)->bReportsReward);

	FOpenMobileAdsLoadRequest LoadRequest;
	LoadRequest.RequestId = FGuid::NewGuid();
	LoadRequest.Placement.Placement = TEXT("ContinueReward");
	LoadRequest.Placement.Format = EOpenMobileAdFormat::Rewarded;
	TestTrue(TEXT("Request identifiers are valid Unreal GUIDs"), LoadRequest.RequestId.IsValid());

	FOpenMobileAdsEvent Event;
	Event.Type = EOpenMobileAdsEventType::RewardEarned;
	Event.Placement = LoadRequest.Placement.Placement;
	Event.Reward.Amount = 10;
	Event.Reward.Type = TEXT("gold");
	Event.bHasReward = true;
	TestEqual(TEXT("Events keep the placement key"), Event.Placement, FName(TEXT("ContinueReward")));
	TestEqual(TEXT("Reward amounts remain integral"), Event.Reward.Amount, static_cast<int64>(10));

	FOpenMobileAdsRevenue Revenue;
	Revenue.ValueMicros = 1234567;
	Revenue.CurrencyCode = TEXT("USD");
	TestEqual(TEXT("Revenue does not lose micro precision"), Revenue.ValueMicros, static_cast<int64>(1234567));

	const FOpenMobileAdsOperationResult Rejected = FOpenMobileAdsOperationResult::Rejected(
		FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::UnsupportedFormat,
			EOpenMobileAdsFailureStage::Load,
			TEXT("ContinueReward"),
			TEXT("The selected provider does not support this format.")
		)
	);
	TestFalse(TEXT("Rejected operations are not accepted"), Rejected.bAccepted);
	TestEqual(TEXT("Rejected operations preserve typed errors"), Rejected.Error.Code, EOpenMobileAdsErrorCode::UnsupportedFormat);
	return true;
}

#endif

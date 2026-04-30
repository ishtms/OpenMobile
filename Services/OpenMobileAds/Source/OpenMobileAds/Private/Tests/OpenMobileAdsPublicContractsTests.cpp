#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsAsyncAction.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsDiagnostics.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsInitialization.h"
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
	Rewarded.bCanHide = false;
	Rewarded.bPreservesCachedAdOnHide = false;
	Rewarded.bReportsReward = true;

	FOpenMobileAdsProviderCapabilities Capabilities;
	Capabilities.Provider = TEXT("MockAds");
	Capabilities.Formats.Add(Rewarded);

	TestTrue(TEXT("Rewarded is supported"), Capabilities.SupportsFormat(EOpenMobileAdFormat::Rewarded));
	TestFalse(TEXT("Interstitial is unsupported"), Capabilities.SupportsFormat(EOpenMobileAdFormat::Interstitial));
	TestTrue(TEXT("Reward support is explicit"), Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded)->bReportsReward);
	TestFalse(TEXT("Hide support is explicit"), Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded)->bCanHide);

	FOpenMobileAdFormatCapabilities RewardedInterstitial = Rewarded;
	RewardedInterstitial.Format = EOpenMobileAdFormat::RewardedInterstitial;
	RewardedInterstitial.bRequiresIntroduction = true;
	Capabilities.Formats.Add(RewardedInterstitial);
	TestNotEqual(
		TEXT("Rewarded interstitial remains distinct from rewarded video"),
		EOpenMobileAdFormat::RewardedInterstitial,
		EOpenMobileAdFormat::Rewarded
	);
	TestTrue(
		TEXT("Rewarded interstitial introduction requirements are explicit"),
		Capabilities.FindFormat(EOpenMobileAdFormat::RewardedInterstitial)
			->bRequiresIntroduction
	);

	FOpenMobileAdsShowOptions RewardedInterstitialShow;
	RewardedInterstitialShow.bRewardedInterstitialIntroductionPresented = true;
	TestTrue(
		TEXT("Show options carry the rewarded-interstitial introduction acknowledgment"),
		RewardedInterstitialShow.bRewardedInterstitialIntroductionPresented
	);

	FOpenMobileAdsPlacementStatus RewardedInterstitialStatus;
	RewardedInterstitialStatus.Format = EOpenMobileAdFormat::RewardedInterstitial;
	RewardedInterstitialStatus.bHasRewardMetadata = true;
	RewardedInterstitialStatus.RewardMetadata.Type = TEXT("coin");
	RewardedInterstitialStatus.RewardMetadata.Amount = 25;
	TestTrue(
		TEXT("Placement status exposes loaded reward metadata"),
		RewardedInterstitialStatus.bHasRewardMetadata
	);
	TestEqual(
		TEXT("Placement status preserves the loaded reward amount"),
		RewardedInterstitialStatus.RewardMetadata.Amount,
		static_cast<int64>(25)
	);

	FOpenMobileAdsHideRequest HideRequest;
	HideRequest.RequestId = FGuid::NewGuid();
	HideRequest.CachedAdId = FGuid::NewGuid();
	HideRequest.Placement = TEXT("MenuBanner");
	HideRequest.Format = EOpenMobileAdFormat::Banner;
	HideRequest.bPreserveCachedAd = true;
	TestTrue(TEXT("Hide requests carry their request identity"), HideRequest.RequestId.IsValid());
	TestTrue(TEXT("Hide requests carry their cache identity"), HideRequest.CachedAdId.IsValid());
	TestTrue(TEXT("Hide requests carry the cache policy"), HideRequest.bPreserveCachedAd);

	FOpenMobileAdsLoadRequest LoadRequest;
	LoadRequest.RequestId = FGuid::NewGuid();
	LoadRequest.Placement.Placement = TEXT("ContinueReward");
	LoadRequest.Placement.Format = EOpenMobileAdFormat::Rewarded;
	TestTrue(TEXT("Request identifiers are valid Unreal GUIDs"), LoadRequest.RequestId.IsValid());

	FOpenMobileAdsInitializationRequest InitializationRequest;
	InitializationRequest.RequestId = FGuid::NewGuid();
	InitializationRequest.Development =
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			true,
			{TEXT("GLOBAL-DEVICE")},
			EOpenMobileAdsDebugGeography::Eea
		);
	InitializationRequest.Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::Yes;
	InitializationRequest.RequestConfiguration.MaxAdContentRating =
		EOpenMobileAdsMaxAdContentRating::ParentalGuidance;
	TestTrue(TEXT("Initialization identifiers use Unreal GUIDs"), InitializationRequest.RequestId.IsValid());
	TestTrue(TEXT("Development mode enables test devices"), InitializationRequest.Development.bUseTestDevices);
	TestTrue(TEXT("Development mode enables official test IDs"), InitializationRequest.Development.bUseTestAdUnitIds);
	TestTrue(TEXT("Development mode enables consent debug controls"), InitializationRequest.Development.bEnableConsentDebug);
	TestTrue(TEXT("Development mode enables verbose diagnostics"), InitializationRequest.Development.bEnableVerboseDiagnostics);
	TestEqual(
		TEXT("Development mode keeps global test-device identifiers"),
		InitializationRequest.Development.TestDeviceIdentifiers,
		TArray<FString>({TEXT("GLOBAL-DEVICE")})
	);
	TestEqual(
		TEXT("Configured test devices enable EEA debug geography"),
		InitializationRequest.Development.GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Eea
	);
	TestEqual(
		TEXT("Development mode without a test device disables debug geography"),
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			true,
			{},
			EOpenMobileAdsDebugGeography::RegulatedUsState
		).GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Disabled
	);
	TestTrue(
		TEXT("Production mode omits configured test-device identifiers"),
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			false,
			{TEXT("GLOBAL-DEVICE")},
			EOpenMobileAdsDebugGeography::Other
		).TestDeviceIdentifiers.IsEmpty()
	);
	TestEqual(
		TEXT("Production mode disables configured debug geography"),
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			false,
			{TEXT("GLOBAL-DEVICE")},
			EOpenMobileAdsDebugGeography::Other
		).GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Disabled
	);
	TestEqual(
		TEXT("Unknown future debug geography values disable simulation"),
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			true,
			{TEXT("GLOBAL-DEVICE")},
			static_cast<EOpenMobileAdsDebugGeography>(255)
		).GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Disabled
	);
	TestNotEqual(
		TEXT("ATT not-determined and unsupported states stay distinct"),
		EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined,
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported
	);
	TestNotEqual(
		TEXT("ATT restricted and denied states stay distinct"),
		EOpenMobileAdsTrackingAuthorizationStatus::Restricted,
		EOpenMobileAdsTrackingAuthorizationStatus::Denied
	);
	TestNotEqual(
		TEXT("ATT authorized and denied states stay distinct"),
		EOpenMobileAdsTrackingAuthorizationStatus::Authorized,
		EOpenMobileAdsTrackingAuthorizationStatus::Denied
	);
	TestNotEqual(
		TEXT("Offline failures stay distinct from generic invalid state"),
		EOpenMobileAdsErrorCode::Offline,
		EOpenMobileAdsErrorCode::InvalidState
	);
	TestEqual(
		TEXT("Initialization keeps provider-neutral request configuration"),
		InitializationRequest.RequestConfiguration.MaxAdContentRating,
		EOpenMobileAdsMaxAdContentRating::ParentalGuidance
	);

	FOpenMobileAdsInitializationStatusSnapshot InitializationStatus;
	FOpenMobileAdsInitializationComponentStatus AdapterStatus;
	AdapterStatus.Type = EOpenMobileAdsInitializationComponentType::Adapter;
	AdapterStatus.Name = TEXT("MockAdapter");
	AdapterStatus.Parent = TEXT("MockAds");
	AdapterStatus.State = EOpenMobileAdsInitializationState::Ready;
	InitializationStatus.Components.Add(AdapterStatus);
	TestNotNull(
		TEXT("Initialization snapshots expose normalized components"),
		InitializationStatus.FindComponent(
			EOpenMobileAdsInitializationComponentType::Adapter,
			TEXT("MockAdapter"),
			TEXT("MockAds")
		)
	);

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
	FOpenMobileAdsCanShowResult CanShowResult;
	CanShowResult.BlockReason = EOpenMobileAdsCanShowBlockReason::Cooldown;
	CanShowResult.NextEligibleAt = FDateTime(2030, 1, 2);
	TestEqual(
		TEXT("Show eligibility preserves its typed reason"),
		CanShowResult.BlockReason,
		EOpenMobileAdsCanShowBlockReason::Cooldown
	);
	TestEqual(
		TEXT("Show eligibility exposes its pacing deadline"),
		CanShowResult.NextEligibleAt,
		FDateTime(2030, 1, 2)
	);
	TestNotNull(
		TEXT("The pacing deadline is reflected for Blueprints"),
		FOpenMobileAdsCanShowResult::StaticStruct()->FindPropertyByName(TEXT("NextEligibleAt"))
	);

	UClass* AsyncActionClass = UOpenMobileAdsAsyncAction::StaticClass();
	TestNotNull(TEXT("Blueprint async action class is reflected"), AsyncActionClass);
#if WITH_METADATA
	TestTrue(TEXT("Blueprint async action exposes its cancellation proxy"), AsyncActionClass->HasMetaData(TEXT("ExposedAsyncProxy")));
#endif
	TestNotNull(TEXT("Completed output is reflected"), AsyncActionClass->FindPropertyByName(TEXT("OnCompleted")));
	TestNotNull(TEXT("Failed output is reflected"), AsyncActionClass->FindPropertyByName(TEXT("OnFailed")));
	TestNotNull(TEXT("Cancelled output is reflected"), AsyncActionClass->FindPropertyByName(TEXT("OnCancelled")));
	TestNotNull(TEXT("Cancellation is callable from the async proxy"), AsyncActionClass->FindFunctionByName(TEXT("Cancel")));
	const FName OperationNames[] = {
		TEXT("LoadAd"),
		TEXT("ShowAd"),
		TEXT("HideAd"),
		TEXT("DestroyAd"),
		TEXT("DestroyAllAds")
	};
	for (FName OperationName : OperationNames)
	{
		const UFunction* Function = AsyncActionClass->FindFunctionByName(OperationName);
		TestNotNull(*FString::Printf(TEXT("%s async node is reflected"), *OperationName.ToString()), Function);
		if (Function)
		{
#if WITH_METADATA
			TestTrue(TEXT("Async factories are hidden behind Blueprint nodes"), Function->HasMetaData(TEXT("BlueprintInternalUseOnly")));
			TestEqual(TEXT("Async factories use an explicit world context"), Function->GetMetaData(TEXT("WorldContext")), FString(TEXT("WorldContextObject")));
#endif
		}
	}
	return true;
}

#endif

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

	FOpenMobileAdsInitializationRequest InitializationRequest;
	InitializationRequest.RequestId = FGuid::NewGuid();
	InitializationRequest.Development =
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			true,
			{TEXT("GLOBAL-DEVICE")}
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
	TestTrue(
		TEXT("Production mode omits configured test-device identifiers"),
		FOpenMobileAdsDevelopmentConfiguration::FromMode(
			false,
			{TEXT("GLOBAL-DEVICE")}
		).TestDeviceIdentifiers.IsEmpty()
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

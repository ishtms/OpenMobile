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

#include <limits>

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

	FOpenMobileAdsAppOpenPresentationState AppOpenPresentation;
	TestFalse(
		TEXT("Automatic app-open presentation waits for application readiness"),
		AppOpenPresentation.bApplicationReady
	);
	TestFalse(
		TEXT("Cold-start app-open presentation requires a visible loading screen"),
		AppOpenPresentation.bColdStartLoadingScreenVisible
	);
	TestFalse(
		TEXT("App-open presentation is not explicitly suppressed by default"),
		AppOpenPresentation.bPresentationSuppressed
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRevenueAmountContractTest,
	"OpenMobile.Ads.Contracts.Revenue.Amount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRevenueAmountContractTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("One major currency unit contains one million micros"),
		FOpenMobileAdsRevenue::MicrosPerMajorUnit,
		static_cast<int64>(1000000)
	);

	int64 ValueMicros = -1;
	TestTrue(
		TEXT("Major currency units convert to exact public micros"),
		FOpenMobileAdsRevenue::TryConvertMajorUnitsToMicros(
			1.234567,
			ValueMicros
		)
	);
	TestEqual(
		TEXT("Major currency conversion preserves six decimal places"),
		ValueMicros,
		static_cast<int64>(1234567)
	);

	ValueMicros = -1;
	TestTrue(
		TEXT("Provider milliunits convert through an explicit scale"),
		FOpenMobileAdsRevenue::TryScaleToMicros(1234, 1000, ValueMicros)
	);
	TestEqual(
		TEXT("Provider scaling produces public micros"),
		ValueMicros,
		static_cast<int64>(1234000)
	);
	TestTrue(
		TEXT("The signed 64-bit boundary remains valid at unit scale"),
		FOpenMobileAdsRevenue::TryScaleToMicros(MAX_int64, 1, ValueMicros)
	);
	TestEqual(
		TEXT("The signed 64-bit boundary does not narrow"),
		ValueMicros,
		static_cast<int64>(MAX_int64)
	);

	const double InvalidMajorValues[] = {
		-0.000001,
		std::numeric_limits<double>::quiet_NaN(),
		std::numeric_limits<double>::infinity(),
		1.0e20
	};
	for (const double InvalidValue : InvalidMajorValues)
	{
		ValueMicros = 77;
		TestFalse(
			TEXT("Invalid major currency values are rejected"),
			FOpenMobileAdsRevenue::TryConvertMajorUnitsToMicros(
				InvalidValue,
				ValueMicros
			)
		);
		TestEqual(
			TEXT("Rejected major currency values clear the output"),
			ValueMicros,
			static_cast<int64>(0)
		);
	}

	for (const TPair<int64, int64>& InvalidScale : {
		TPair<int64, int64>(-1, 1),
		TPair<int64, int64>(1, 0),
		TPair<int64, int64>(1, -1),
		TPair<int64, int64>(MAX_int64, 2)
	})
	{
		ValueMicros = 77;
		TestFalse(
			TEXT("Invalid provider-scaled values are rejected"),
			FOpenMobileAdsRevenue::TryScaleToMicros(
				InvalidScale.Key,
				InvalidScale.Value,
				ValueMicros
			)
		);
		TestEqual(
			TEXT("Rejected provider-scaled values clear the output"),
			ValueMicros,
			static_cast<int64>(0)
		);
	}

	const FProperty* ValueProperty =
		FOpenMobileAdsRevenue::StaticStruct()->FindPropertyByName(TEXT("ValueMicros"));
	TestNotNull(TEXT("Revenue micros are reflected"), ValueProperty);
	if (ValueProperty)
	{
		TestTrue(
			TEXT("Revenue micros are visible to Blueprint"),
			ValueProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)
		);
	}

	FOpenMobileAdsEvent MissingRevenue;
	TestFalse(
		TEXT("A missing revenue amount remains distinct from zero"),
		MissingRevenue.bHasRevenue
	);
	FOpenMobileAdsEvent ZeroRevenue;
	ZeroRevenue.bHasRevenue = true;
	ZeroRevenue.Revenue.ValueMicros = 0;
	TestTrue(TEXT("A reported zero revenue amount remains present"), ZeroRevenue.bHasRevenue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRevenueCurrencyContractTest,
	"OpenMobile.Ads.Contracts.Revenue.Currency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRevenueCurrencyContractTest::RunTest(const FString& Parameters)
{
	FString CurrencyCode = TEXT("stale");
	TestTrue(
		TEXT("Lowercase ISO currency codes are accepted"),
		FOpenMobileAdsRevenue::TryNormalizeCurrencyCode(TEXT("usd"), CurrencyCode)
	);
	TestTrue(
		TEXT("Currency codes are normalized to uppercase"),
		CurrencyCode.Equals(TEXT("USD"), ESearchCase::CaseSensitive)
	);
	CurrencyCode = TEXT("gBp");
	TestTrue(
		TEXT("Currency normalization supports in-place use"),
		FOpenMobileAdsRevenue::TryNormalizeCurrencyCode(CurrencyCode, CurrencyCode)
	);
	TestTrue(
		TEXT("In-place currency normalization preserves the normalized value"),
		CurrencyCode.Equals(TEXT("GBP"), ESearchCase::CaseSensitive)
	);

	CurrencyCode = TEXT("stale");
	TestFalse(
		TEXT("Missing currency remains unavailable"),
		FOpenMobileAdsRevenue::TryNormalizeCurrencyCode(TEXT(""), CurrencyCode)
	);
	TestTrue(TEXT("Missing currency does not invent a value"), CurrencyCode.IsEmpty());

	const TCHAR* MalformedCodes[] = {
		TEXT("US"),
		TEXT("USDD"),
		TEXT("U1D"),
		TEXT(" USD"),
		TEXT("\u20acUR")
	};
	for (const TCHAR* MalformedCode : MalformedCodes)
	{
		CurrencyCode = TEXT("stale");
		TestFalse(
			TEXT("Malformed currency codes are rejected"),
			FOpenMobileAdsRevenue::TryNormalizeCurrencyCode(
				MalformedCode,
				CurrencyCode
			)
		);
		TestTrue(
			TEXT("Malformed currency codes remain unavailable"),
			CurrencyCode.IsEmpty()
		);
	}

	const FProperty* CurrencyProperty =
		FOpenMobileAdsRevenue::StaticStruct()->FindPropertyByName(TEXT("CurrencyCode"));
	TestNotNull(TEXT("Revenue currency is reflected"), CurrencyProperty);
	if (CurrencyProperty)
	{
		TestTrue(
			TEXT("Revenue currency is visible to Blueprint"),
			CurrencyProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)
		);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRevenuePrecisionContractTest,
	"OpenMobile.Ads.Contracts.Revenue.Precision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRevenuePrecisionContractTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdsRevenue Revenue;
	TestEqual(
		TEXT("Revenue precision defaults to unknown"),
		Revenue.Precision,
		EOpenMobileAdsRevenuePrecision::Unknown
	);
	TestNotEqual(
		TEXT("Estimated revenue stays distinct from unknown"),
		EOpenMobileAdsRevenuePrecision::Estimated,
		EOpenMobileAdsRevenuePrecision::Unknown
	);
	TestNotEqual(
		TEXT("Publisher-provided revenue stays distinct from estimates"),
		EOpenMobileAdsRevenuePrecision::PublisherProvided,
		EOpenMobileAdsRevenuePrecision::Estimated
	);
	TestNotEqual(
		TEXT("Precise revenue stays distinct from publisher-provided values"),
		EOpenMobileAdsRevenuePrecision::Precise,
		EOpenMobileAdsRevenuePrecision::PublisherProvided
	);
	const UEnum* PrecisionEnum = StaticEnum<EOpenMobileAdsRevenuePrecision>();
	TestNotNull(TEXT("Revenue precision is a reflected enum"), PrecisionEnum);
	if (PrecisionEnum)
	{
		TestEqual(
			TEXT("Blueprint describes precise provider values as exact"),
			PrecisionEnum->GetDisplayNameTextByValue(
				static_cast<int64>(EOpenMobileAdsRevenuePrecision::Precise)
			).ToString(),
			FString(TEXT("Exact"))
		);
	}

	const FProperty* PrecisionProperty =
		FOpenMobileAdsRevenue::StaticStruct()->FindPropertyByName(TEXT("Precision"));
	TestNotNull(TEXT("Revenue precision is reflected"), PrecisionProperty);
	if (PrecisionProperty)
	{
		TestTrue(
			TEXT("Revenue precision is visible to Blueprint"),
			PrecisionProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)
		);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRevenueSourceContractTest,
	"OpenMobile.Ads.Contracts.Revenue.Source",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRevenueSourceContractTest::RunTest(const FString& Parameters)
{
	struct FSourceCase
	{
		const TCHAR* Label;
		FOpenMobileAdsRevenueSource Source;
		bool bExpectedEmpty;
	};

	FSourceCase Cases[] = {
		{
			TEXT("Direct"),
			{
				TEXT("Google Ads"),
				TEXT("5450213213286189855"),
				TEXT("com.google.ads.mediation.admob.AdMobAdapter"),
				TEXT("AdMob Network"),
				TEXT("4665218928925097")
			},
			false
		},
		{
			TEXT("Mediated"),
			{
				TEXT("Example Network"),
				TEXT("source-42"),
				TEXT("com.example.ads.Adapter"),
				TEXT("Example waterfall"),
				TEXT("instance-7")
			},
			false
		},
		{
			TEXT("Bidding"),
			{
				TEXT("Bidder Network"),
				TEXT("bidder-source"),
				TEXT("com.example.bidder.Adapter"),
				TEXT(""),
				TEXT("")
			},
			false
		},
		{
			TEXT("Waterfall"),
			{
				TEXT("Waterfall Network"),
				TEXT(""),
				TEXT("com.example.waterfall.Adapter"),
				TEXT("Waterfall instance"),
				TEXT("instance-waterfall")
			},
			false
		},
		{TEXT("Unknown"), {}, true}
	};

	for (const FSourceCase& SourceCase : Cases)
	{
		FOpenMobileAdsRevenue Revenue;
		Revenue.Source = SourceCase.Source;
		Revenue.NormalizeSource();
		TestEqual(
			FString::Printf(TEXT("%s source emptiness is explicit"), SourceCase.Label),
			Revenue.Source.IsEmpty(),
			SourceCase.bExpectedEmpty
		);
		TestEqual(
			FString::Printf(TEXT("%s source name remains exact"), SourceCase.Label),
			Revenue.Source.SourceName,
			SourceCase.Source.SourceName
		);
		TestEqual(
			FString::Printf(TEXT("%s source ID remains exact"), SourceCase.Label),
			Revenue.Source.SourceId,
			SourceCase.Source.SourceId
		);
		TestEqual(
			FString::Printf(TEXT("%s adapter remains exact"), SourceCase.Label),
			Revenue.Source.AdapterClassName,
			SourceCase.Source.AdapterClassName
		);
		TestEqual(
			FString::Printf(TEXT("%s instance name remains exact"), SourceCase.Label),
			Revenue.Source.InstanceName,
			SourceCase.Source.InstanceName
		);
		TestEqual(
			FString::Printf(TEXT("%s instance ID remains exact"), SourceCase.Label),
			Revenue.Source.InstanceId,
			SourceCase.Source.InstanceId
		);
		TestEqual(
			FString::Printf(TEXT("%s network alias is normalized"), SourceCase.Label),
			Revenue.Network,
			SourceCase.Source.SourceName
		);
	}

	TestTrue(
		TEXT("Missing source IDs remain unavailable"),
		Cases[2].Source.InstanceId.IsEmpty()
	);
	TestTrue(
		TEXT("A source display name is not parsed into an ID"),
		Cases[3].Source.SourceId.IsEmpty()
	);

	const FProperty* SourceProperty =
		FOpenMobileAdsRevenue::StaticStruct()->FindPropertyByName(TEXT("Source"));
	TestNotNull(TEXT("Revenue source is reflected"), SourceProperty);
	if (SourceProperty)
	{
		TestTrue(
			TEXT("Revenue source is visible to Blueprint"),
			SourceProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)
		);
	}

	for (const FName PropertyName : {
		FName(TEXT("SourceName")),
		FName(TEXT("SourceId")),
		FName(TEXT("AdapterClassName")),
		FName(TEXT("InstanceName")),
		FName(TEXT("InstanceId"))
	})
	{
		const FProperty* Property =
			FOpenMobileAdsRevenueSource::StaticStruct()->FindPropertyByName(PropertyName);
		TestNotNull(
			FString::Printf(TEXT("Revenue source field %s is reflected"), *PropertyName.ToString()),
			Property
		);
		if (Property)
		{
			TestTrue(
				TEXT("Revenue source fields are visible to Blueprint"),
				Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsImpressionRevenueContractTest,
	"OpenMobile.Ads.Contracts.Revenue.ImpressionLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsImpressionRevenueContractTest::RunTest(
	const FString& Parameters
)
{
	FOpenMobileAdsEvent Event;
	TestFalse(
		TEXT("Events do not invent an impression identity"),
		Event.ImpressionId.IsValid()
	);
	TestEqual(
		TEXT("Unreported revenue has no revision"),
		Event.Revenue.Revision,
		0
	);
	TestFalse(
		TEXT("The first revenue report is not an update"),
		Event.Revenue.bIsUpdate
	);

	const FProperty* ImpressionIdProperty =
		FOpenMobileAdsEvent::StaticStruct()->FindPropertyByName(TEXT("ImpressionId"));
	TestNotNull(TEXT("Impression identity is reflected"), ImpressionIdProperty);
	if (ImpressionIdProperty)
	{
		TestTrue(
			TEXT("Impression identity is visible to Blueprint"),
			ImpressionIdProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)
		);
	}

	for (const FName PropertyName : {
		FName(TEXT("Revision")),
		FName(TEXT("bIsUpdate"))
	})
	{
		const FProperty* Property =
			FOpenMobileAdsRevenue::StaticStruct()->FindPropertyByName(PropertyName);
		TestNotNull(
			FString::Printf(TEXT("ILRD field %s is reflected"), *PropertyName.ToString()),
			Property
		);
		if (Property)
		{
			TestTrue(
				TEXT("ILRD fields are visible to Blueprint"),
				Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsEcpmContractTest,
	"OpenMobile.Ads.Contracts.Revenue.Ecpm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsEcpmContractTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdsRevenue Revenue;
	TestFalse(
		TEXT("Provider eCPM is unavailable by default"),
		Revenue.Ecpm.bHasProviderReported
	);
	TestFalse(
		TEXT("Derived eCPM is unavailable by default"),
		Revenue.Ecpm.bHasDerived
	);

	Revenue.ValueMicros = 1234;
	Revenue.CurrencyCode = TEXT("USD");
	Revenue.Precision = EOpenMobileAdsRevenuePrecision::Estimated;
	int64 ProviderEcpmMicros = 0;
	TestTrue(
		TEXT("Provider eCPM converts through an explicit unit scale"),
		FOpenMobileAdsRevenue::TryScaleToMicros(
			2345,
			1000,
			ProviderEcpmMicros
		)
	);
	Revenue.Ecpm.bHasProviderReported = true;
	Revenue.Ecpm.ProviderReported.ValueMicros = ProviderEcpmMicros;
	Revenue.Ecpm.ProviderReported.CurrencyCode = TEXT("eUr");
	Revenue.Ecpm.ProviderReported.Precision =
		EOpenMobileAdsRevenuePrecision::Precise;
	Revenue.NormalizeEcpm();
	TestTrue(
		TEXT("A valid provider eCPM remains available"),
		Revenue.Ecpm.bHasProviderReported
	);
	TestEqual(
		TEXT("Provider eCPM remains in public micros"),
		Revenue.Ecpm.ProviderReported.ValueMicros,
		static_cast<int64>(2345000)
	);
	TestEqual(
		TEXT("Provider eCPM currency is normalized independently"),
		Revenue.Ecpm.ProviderReported.CurrencyCode,
		FString(TEXT("EUR"))
	);
	TestEqual(
		TEXT("Provider eCPM precision is preserved"),
		Revenue.Ecpm.ProviderReported.Precision,
		EOpenMobileAdsRevenuePrecision::Precise
	);
	TestTrue(
		TEXT("ILRD derives a separate eCPM value"),
		Revenue.Ecpm.bHasDerived
	);
	TestEqual(
		TEXT("Derived eCPM multiplies per-impression micros by one thousand"),
		Revenue.Ecpm.Derived.ValueMicros,
		static_cast<int64>(1234000)
	);
	TestEqual(
		TEXT("Derived eCPM inherits ILRD currency"),
		Revenue.Ecpm.Derived.CurrencyCode,
		FString(TEXT("USD"))
	);
	TestEqual(
		TEXT("Derived eCPM inherits ILRD precision"),
		Revenue.Ecpm.Derived.Precision,
		EOpenMobileAdsRevenuePrecision::Estimated
	);

	Revenue.ValueMicros = MAX_int64;
	Revenue.Ecpm.bHasProviderReported = false;
	Revenue.Ecpm.ProviderReported.ValueMicros = 77;
	Revenue.NormalizeEcpm();
	TestFalse(
		TEXT("Overflowing derived eCPM remains unavailable"),
		Revenue.Ecpm.bHasDerived
	);
	TestEqual(
		TEXT("Unavailable derived eCPM clears stale values"),
		Revenue.Ecpm.Derived.ValueMicros,
		static_cast<int64>(0)
	);
	TestEqual(
		TEXT("Unavailable provider eCPM clears stale values"),
		Revenue.Ecpm.ProviderReported.ValueMicros,
		static_cast<int64>(0)
	);

	const FProperty* EcpmProperty =
		FOpenMobileAdsRevenue::StaticStruct()->FindPropertyByName(TEXT("Ecpm"));
	TestNotNull(TEXT("eCPM is reflected"), EcpmProperty);
	if (EcpmProperty)
	{
		TestTrue(
			TEXT("eCPM is visible to Blueprint"),
			EcpmProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)
		);
	}
	for (const FName PropertyName : {
		FName(TEXT("bHasProviderReported")),
		FName(TEXT("ProviderReported")),
		FName(TEXT("bHasDerived")),
		FName(TEXT("Derived"))
	})
	{
		const FProperty* Property =
			FOpenMobileAdsEcpm::StaticStruct()->FindPropertyByName(PropertyName);
		TestNotNull(
			FString::Printf(TEXT("eCPM field %s is reflected"), *PropertyName.ToString()),
			Property
		);
		if (Property)
		{
			TestTrue(
				TEXT("eCPM fields are visible to Blueprint"),
				Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			);
		}
	}
	for (const FName PropertyName : {
		FName(TEXT("ValueMicros")),
		FName(TEXT("CurrencyCode")),
		FName(TEXT("Precision"))
	})
	{
		const FProperty* Property =
			FOpenMobileAdsEcpmValue::StaticStruct()->FindPropertyByName(PropertyName);
		TestNotNull(
			FString::Printf(TEXT("eCPM value field %s is reflected"), *PropertyName.ToString()),
			Property
		);
		if (Property)
		{
			TestTrue(
				TEXT("eCPM value fields are visible to Blueprint"),
				Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsServerVerificationPublicContractTest,
	"OpenMobile.Ads.Contracts.Reward.ServerVerification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsServerVerificationPublicContractTest::RunTest(
	const FString& Parameters
)
{
	for (const FName PropertyName : {
		FName(TEXT("bEnabled")),
		FName(TEXT("bRequireUserId")),
		FName(TEXT("bRequireCustomData"))
	})
	{
		const FProperty* Property =
			FOpenMobileAdsServerVerificationSettings::StaticStruct()
				->FindPropertyByName(PropertyName);
		TestNotNull(
			FString::Printf(
				TEXT("Server verification setting %s is reflected"),
				*PropertyName.ToString()
			),
			Property
		);
		if (Property)
		{
			TestTrue(
				TEXT("Server verification settings are visible to Blueprint"),
				Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			);
		}
	}

	for (const FName PropertyName : {
		FName(TEXT("ServerVerificationUserId")),
		FName(TEXT("ServerVerificationCustomData"))
	})
	{
		const FProperty* Property =
			FOpenMobileAdsShowOptions::StaticStruct()->FindPropertyByName(PropertyName);
		TestNotNull(
			FString::Printf(
				TEXT("Show option %s is reflected"),
				*PropertyName.ToString()
			),
			Property
		);
		if (Property)
		{
			TestTrue(
				TEXT("Server verification show options are visible to Blueprint"),
				Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			);
		}
	}

	const FProperty* RewardRequestedProperty =
		FOpenMobileAdsReward::StaticStruct()
			->FindPropertyByName(TEXT("bServerVerificationRequested"));
	TestNotNull(
		TEXT("Server verification requested state is reflected"),
		RewardRequestedProperty
	);
	if (RewardRequestedProperty)
	{
		TestTrue(
			TEXT("Server verification requested state is visible to Blueprint"),
			RewardRequestedProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)
		);
	}

	for (const FName PropertyName : {
		FName(TEXT("bServerVerified")),
		FName(TEXT("VerificationId"))
	})
	{
		const FProperty* Property =
			FOpenMobileAdsReward::StaticStruct()->FindPropertyByName(PropertyName);
		TestNotNull(
			FString::Printf(
				TEXT("Legacy reward field %s remains reflected"),
				*PropertyName.ToString()
			),
			Property
		);
		if (Property)
		{
		#if WITH_EDITORONLY_DATA
			TestTrue(
				TEXT("Legacy local verification fields are deprecated"),
				Property->HasMetaData(TEXT("DeprecatedProperty"))
			);
		#endif
		}
	}
	return true;
}

#endif

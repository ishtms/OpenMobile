#include "Misc/AutomationTest.h"
#include "OpenMobileAdsErrors.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsKnownErrorMappingTest,
	"OpenMobile.Ads.Errors.KnownMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsKnownErrorMappingTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdsErrorMappingContext ProviderContext;
	ProviderContext.Domain = EOpenMobileAdsErrorDomain::Provider;
	ProviderContext.Stage = EOpenMobileAdsFailureStage::Load;
	ProviderContext.Placement = TEXT("ContinueReward");
	ProviderContext.Provider = TEXT("MockAds");
	ProviderContext.NativeCode = TEXT("NO_FILL");
	ProviderContext.NativeMessage = TEXT("ad_unit_id=private-unit no inventory");
	const FOpenMobileAdsError NoFill = FOpenMobileAdsErrorMapper::FromNative(
		ProviderContext,
		{TEXT("private-unit")}
	);
	TestEqual(TEXT("No fill uses a distinct nonfatal error"), NoFill.Code, EOpenMobileAdsErrorCode::NoFill);
	TestEqual(TEXT("The failed stage is preserved"), NoFill.Stage, EOpenMobileAdsFailureStage::Load);
	TestEqual(TEXT("The placement is preserved"), NoFill.Placement, FName(TEXT("ContinueReward")));
	TestEqual(TEXT("The provider is preserved"), NoFill.Provider, FName(TEXT("MockAds")));
	TestTrue(TEXT("No fill can be retried"), NoFill.bRetryable);
	TestFalse(TEXT("Known errors explain the likely cause"), NoFill.LikelyCause.IsEmpty());
	TestFalse(TEXT("Known errors suggest a correction"), NoFill.SuggestedCorrection.IsEmpty());
	TestEqual(TEXT("Native codes remain available"), NoFill.NativeDiagnostics.NativeCode, FString(TEXT("NO_FILL")));
	TestFalse(TEXT("Sensitive native details are redacted"), NoFill.NativeDiagnostics.NativeMessage.Contains(TEXT("private-unit")));

	FOpenMobileAdsErrorMappingContext MediatedNoFillContext;
	MediatedNoFillContext.Domain = EOpenMobileAdsErrorDomain::Mediation;
	MediatedNoFillContext.Stage = EOpenMobileAdsFailureStage::Load;
	MediatedNoFillContext.Placement = TEXT("ContinueReward");
	MediatedNoFillContext.Provider = TEXT("MockAds");
	MediatedNoFillContext.Network = TEXT("MockNetwork");
	MediatedNoFillContext.Adapter = TEXT("MockAdapter");
	MediatedNoFillContext.NativeCode = TEXT("no_fill");
	const FOpenMobileAdsError MediatedNoFill =
		FOpenMobileAdsErrorMapper::FromNative(MediatedNoFillContext);
	TestEqual(
		TEXT("Mediated no fill keeps the normalized error"),
		MediatedNoFill.Code,
		EOpenMobileAdsErrorCode::NoFill
	);
	TestEqual(
		TEXT("Mediated no fill keeps the network"),
		MediatedNoFill.NativeDiagnostics.Network,
		FString(TEXT("MockNetwork"))
	);
	TestEqual(
		TEXT("Mediated no fill keeps the adapter"),
		MediatedNoFill.NativeDiagnostics.Adapter,
		FString(TEXT("MockAdapter"))
	);

	FOpenMobileAdsErrorMappingContext MediationContext;
	MediationContext.Domain = EOpenMobileAdsErrorDomain::Mediation;
	MediationContext.Stage = EOpenMobileAdsFailureStage::Initialization;
	MediationContext.Provider = TEXT("MockAds");
	MediationContext.Network = TEXT("MockNetwork");
	MediationContext.Adapter = TEXT("MockAdapter");
	MediationContext.NativeCode = TEXT("adapter_not_ready");
	const FOpenMobileAdsError Adapter = FOpenMobileAdsErrorMapper::FromNative(MediationContext);
	TestEqual(TEXT("Adapter readiness is normalized"), Adapter.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);
	TestEqual(TEXT("Mediation network is preserved"), Adapter.NativeDiagnostics.Network, FString(TEXT("MockNetwork")));
	TestEqual(TEXT("Mediation adapter is preserved"), Adapter.NativeDiagnostics.Adapter, FString(TEXT("MockAdapter")));

	FOpenMobileAdsErrorMappingContext ConsentContext;
	ConsentContext.Domain = EOpenMobileAdsErrorDomain::Consent;
	ConsentContext.Stage = EOpenMobileAdsFailureStage::Consent;
	ConsentContext.NativeCode = TEXT("consent_required");
	const FOpenMobileAdsError Consent = FOpenMobileAdsErrorMapper::FromNative(ConsentContext);
	TestEqual(TEXT("Consent requirements block requests"), Consent.Code, EOpenMobileAdsErrorCode::PrivacyBlocked);

	FOpenMobileAdsErrorMappingContext PackagingContext;
	PackagingContext.Domain = EOpenMobileAdsErrorDomain::Packaging;
	PackagingContext.Stage = EOpenMobileAdsFailureStage::Initialization;
	PackagingContext.Provider = TEXT("MockAds");
	PackagingContext.NativeCode = TEXT("missing_app_id");
	const FOpenMobileAdsError Packaging = FOpenMobileAdsErrorMapper::FromNative(PackagingContext);
	TestEqual(TEXT("Missing packaged configuration is normalized"), Packaging.Code, EOpenMobileAdsErrorCode::NotConfigured);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRetryClassificationTest,
	"OpenMobile.Ads.Reliability.Retry.Classification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRetryClassificationTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdsError NoFill;
	NoFill.Code = EOpenMobileAdsErrorCode::NoFill;
	TestEqual(
		TEXT("No fill is retryable"),
		FOpenMobileAdsErrorClassifier::Classify(NoFill),
		EOpenMobileAdsRetryClassification::Retryable
	);

	FOpenMobileAdsError NativeFailure;
	NativeFailure.Code = EOpenMobileAdsErrorCode::NativeFailure;
	NativeFailure.bRetryable = true;
	TestEqual(
		TEXT("Provider-marked transient native errors are retryable"),
		FOpenMobileAdsErrorClassifier::Classify(NativeFailure),
		EOpenMobileAdsRetryClassification::Retryable
	);

	FOpenMobileAdsError Offline;
	Offline.Code = EOpenMobileAdsErrorCode::Offline;
	Offline.bRetryable = true;
	TestEqual(
		TEXT("Offline errors require an external condition"),
		FOpenMobileAdsErrorClassifier::Classify(Offline),
		EOpenMobileAdsRetryClassification::ConditionallyRetryable
	);

	FOpenMobileAdsError PrivacyBlocked;
	PrivacyBlocked.Code = EOpenMobileAdsErrorCode::PrivacyBlocked;
	TestEqual(
		TEXT("Privacy blocks require a new eligibility decision"),
		FOpenMobileAdsErrorClassifier::Classify(PrivacyBlocked),
		EOpenMobileAdsRetryClassification::ConditionallyRetryable
	);

	FOpenMobileAdsError NotConfigured;
	NotConfigured.Code = EOpenMobileAdsErrorCode::NotConfigured;
	NotConfigured.bRetryable = true;
	TestEqual(
		TEXT("Configuration errors remain terminal"),
		FOpenMobileAdsErrorClassifier::Classify(NotConfigured),
		EOpenMobileAdsRetryClassification::Terminal
	);

	FOpenMobileAdsError NonRetryableNativeFailure;
	NonRetryableNativeFailure.Code = EOpenMobileAdsErrorCode::NativeFailure;
	TestEqual(
		TEXT("Native failures require an explicit retry signal"),
		FOpenMobileAdsErrorClassifier::Classify(NonRetryableNativeFailure),
		EOpenMobileAdsRetryClassification::Terminal
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsUnknownErrorMappingTest,
	"OpenMobile.Ads.Errors.UnknownMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsUnknownErrorMappingTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdsErrorMappingContext Context;
	Context.Domain = EOpenMobileAdsErrorDomain::Provider;
	Context.Stage = EOpenMobileAdsFailureStage::Show;
	Context.Placement = TEXT("ContinueReward");
	Context.Provider = TEXT("FutureAds");
	Context.NativeCode = TEXT("future_code_9000");
	const FOpenMobileAdsError Error = FOpenMobileAdsErrorMapper::FromNative(Context);
	TestEqual(TEXT("Unknown native errors stay typed"), Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	TestEqual(TEXT("Unknown native codes are preserved"), Error.NativeDiagnostics.NativeCode, FString(TEXT("future_code_9000")));
	TestTrue(TEXT("Missing native messages remain empty"), Error.NativeDiagnostics.NativeMessage.IsEmpty());
	TestFalse(TEXT("Unknown errors still have an explanation"), Error.Explanation.IsEmpty());
	TestFalse(TEXT("Unknown errors still suggest a correction"), Error.SuggestedCorrection.IsEmpty());
	return true;
}

#endif

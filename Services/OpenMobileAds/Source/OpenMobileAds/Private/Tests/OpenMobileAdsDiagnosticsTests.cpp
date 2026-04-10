#include "Misc/AutomationTest.h"
#include "OpenMobileAdsDiagnostics.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsLogFilterTest,
	"OpenMobile.Ads.Diagnostics.LogFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsLogFilterTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Global warning level includes errors"),
		FOpenMobileAdsLog::IsLevelEnabled(EOpenMobileAdsLogLevel::Error, 1, -1)
	);
	TestTrue(
		TEXT("Global warning level includes warnings"),
		FOpenMobileAdsLog::IsLevelEnabled(EOpenMobileAdsLogLevel::Warning, 1, -1)
	);
	TestFalse(
		TEXT("Global warning level filters info"),
		FOpenMobileAdsLog::IsLevelEnabled(EOpenMobileAdsLogLevel::Info, 1, -1)
	);
	TestTrue(
		TEXT("Ads-specific level overrides the global level"),
		FOpenMobileAdsLog::IsLevelEnabled(EOpenMobileAdsLogLevel::Verbose, 0, 3)
	);
	TestFalse(
		TEXT("Negative global level disables inherited ads logs"),
		FOpenMobileAdsLog::IsLevelEnabled(EOpenMobileAdsLogLevel::Error, -1, -1)
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsLogRedactionTest,
	"OpenMobile.Ads.Diagnostics.Redaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsLogRedactionTest::RunTest(const FString& Parameters)
{
	const FString SensitiveAdUnit = TEXT("ca-app-pub-123456/987654");
	const FString Redacted = FOpenMobileAdsLog::Redact(
		TEXT("ad_unit_id=ca-app-pub-123456/987654 deviceId:device-42 consent_data=granted custom-data='reward-user' advertisingId=id-77 IDFA=ios-88 safe=kept"),
		{SensitiveAdUnit}
	);
	TestFalse(TEXT("Ad-unit IDs are redacted"), Redacted.Contains(SensitiveAdUnit));
	TestFalse(TEXT("Device IDs are redacted"), Redacted.Contains(TEXT("device-42")));
	TestFalse(TEXT("Consent data is redacted"), Redacted.Contains(TEXT("granted")));
	TestFalse(TEXT("Custom data is redacted"), Redacted.Contains(TEXT("reward-user")));
	TestFalse(TEXT("Advertising IDs are redacted"), Redacted.Contains(TEXT("id-77")));
	TestFalse(TEXT("IDFA values are redacted"), Redacted.Contains(TEXT("ios-88")));
	TestTrue(TEXT("Unrelated log context is preserved"), Redacted.Contains(TEXT("safe=kept")));

	FOpenMobileAdsNativeDiagnostics Diagnostics;
	Diagnostics.NativeCode = TEXT("native-7");
	Diagnostics.NativeMessage = TEXT("gaid=android-99 adUnitId=unit-1");
	const FOpenMobileAdsNativeDiagnostics SafeDiagnostics =
		FOpenMobileAdsLog::Redact(Diagnostics, {TEXT("unit-1")});
	TestEqual(TEXT("Native codes are preserved"), SafeDiagnostics.NativeCode, FString(TEXT("native-7")));
	TestFalse(TEXT("Native messages redact advertising identifiers"), SafeDiagnostics.NativeMessage.Contains(TEXT("android-99")));
	TestFalse(TEXT("Native messages redact explicit identifiers"), SafeDiagnostics.NativeMessage.Contains(TEXT("unit-1")));
	return true;
}

#endif

#include "OpenMobileAdsAdMobSettings.h"
#include "OpenMobileAdsAdMobSettingsValidator.h"
#include "OpenMobileAdsTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobSettingsValidatorTest,
	"OpenMobile.Ads.AdMob.SettingsValidator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobSettingsValidatorTest::RunTest(const FString& Parameters)
{
	UOpenMobileAdsAdMobSettings* Settings = NewObject<UOpenMobileAdsAdMobSettings>();
	TestNotNull(TEXT("Settings can be created"), Settings);
	if (!Settings)
	{
		return false;
	}

	TestEqual(
		TEXT("Default test identifiers are valid"),
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings).Num(),
		0
	);
	TestEqual(
		TEXT("Android development mode uses Google's rewarded test ID"),
		Settings->ResolveRewardedAdUnitId(EOpenMobileAdsPlatform::Android, true),
		FString(TEXT("ca-app-pub-3940256099942544/5224354917"))
	);
	TestEqual(
		TEXT("iOS development mode uses Google's rewarded test ID"),
		Settings->ResolveRewardedAdUnitId(EOpenMobileAdsPlatform::IOS, true),
		FString(TEXT("ca-app-pub-3940256099942544/1712485313"))
	);
	Settings->AndroidRewardedAdUnitId = TEXT("ca-app-pub-1234567890123456/1234567890");
	TestEqual(
		TEXT("Production mode keeps the configured Android ID"),
		Settings->ResolveRewardedAdUnitId(EOpenMobileAdsPlatform::Android, false),
		Settings->AndroidRewardedAdUnitId
	);
	Settings->AndroidRewardedAdUnitId = TEXT("ca-app-pub-3940256099942544/5224354917");
	TestTrue(
		TEXT("Shipping validation rejects Google's known sample IDs"),
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings, true).Contains(
			TEXT("Google sample IDs are not allowed in shipping builds.")
		)
	);

	Settings->AndroidAppId = TEXT("  ca-app-pub-invalid~123");
	const TArray<FString> WhitespaceErrors =
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings);
	TestTrue(
		TEXT("Whitespace is rejected"),
		WhitespaceErrors.Contains(TEXT("Android app ID must not contain whitespace."))
	);

	Settings->AndroidAppId = TEXT("ca-app-pub-invalid/123");
	const TArray<FString> SeparatorErrors =
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings);
	TestTrue(
		TEXT("App IDs require their app separator"),
		SeparatorErrors.Contains(TEXT("Android app ID must contain '~'."))
	);

	Settings->AndroidAppId.Reset();
	const TArray<FString> MissingErrors =
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings);
	TestTrue(
		TEXT("Missing IDs are rejected once"),
		MissingErrors.Contains(TEXT("Android app ID is required."))
	);
	return true;
}

#endif

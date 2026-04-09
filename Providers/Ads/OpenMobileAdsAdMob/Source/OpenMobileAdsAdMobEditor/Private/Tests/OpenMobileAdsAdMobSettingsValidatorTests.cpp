#include "OpenMobileAdsAdMobSettings.h"
#include "OpenMobileAdsAdMobSettingsValidator.h"

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

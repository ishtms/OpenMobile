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
		TEXT("AdMob settings use the OpenMobile category"),
		Settings->GetCategoryName(),
		FName(TEXT("OpenMobile"))
	);
	TestEqual(
		TEXT("AdMob settings keep their own section"),
		Settings->GetSectionName(),
		FName(TEXT("OpenMobile - AdMob"))
	);
	TestEqual(
		TEXT("AdMob settings display their section name"),
		Settings->GetClass()->GetMetaData(TEXT("DisplayName")),
		FString(TEXT("OpenMobile - AdMob"))
	);
	TestEqual(
		TEXT("Default test identifiers are valid"),
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings).Num(),
		0
	);
	TestTrue(
		TEXT("AdMob test-device identifiers are empty by default"),
		Settings->TestDeviceIdentifiers.IsEmpty()
	);
	Settings->TestDeviceIdentifiers = {
		TEXT("SHARED-DEVICE"),
		TEXT("ADMOB-DEVICE")
	};
	TestEqual(
		TEXT("AdMob merges global and provider test devices without duplicates"),
		Settings->ResolveTestDeviceIdentifiers({
			TEXT("GLOBAL-DEVICE"),
			TEXT("shared-device")
		}),
		TArray<FString>({
			TEXT("GLOBAL-DEVICE"),
			TEXT("shared-device"),
			TEXT("ADMOB-DEVICE")
		})
	);
	Settings->TestDeviceIdentifiers = {TEXT("invalid device")};
	TestTrue(
		TEXT("Malformed AdMob test-device identifiers are rejected"),
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings).Contains(
			TEXT("AdMob test-device identifier at index 0 is invalid.")
		)
	);
	Settings->TestDeviceIdentifiers = {TEXT("ADMOB-DEVICE")};
	TestTrue(
		TEXT("AdMob test-device identifiers are rejected for shipping"),
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings, true).Contains(
			TEXT("AdMob test-device identifiers are not allowed in shipping builds.")
		)
	);
	Settings->TestDeviceIdentifiers.Reset();
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
	TestEqual(
		TEXT("Android development mode uses Google's interstitial test ID"),
		Settings->ResolveInterstitialAdUnitId(EOpenMobileAdsPlatform::Android, true),
		FString(TEXT("ca-app-pub-3940256099942544/1033173712"))
	);
	TestEqual(
		TEXT("iOS development mode uses Google's interstitial test ID"),
		Settings->ResolveInterstitialAdUnitId(EOpenMobileAdsPlatform::IOS, true),
		FString(TEXT("ca-app-pub-3940256099942544/4411468910"))
	);
	TestEqual(
		TEXT("Android development mode uses Google's fixed-banner test ID"),
		Settings->ResolveBannerAdUnitId(EOpenMobileAdsPlatform::Android, true),
		FString(TEXT("ca-app-pub-3940256099942544/6300978111"))
	);
	TestEqual(
		TEXT("iOS development mode uses Google's fixed-banner test ID"),
		Settings->ResolveBannerAdUnitId(EOpenMobileAdsPlatform::IOS, true),
		FString(TEXT("ca-app-pub-3940256099942544/2435281174"))
	);
	TestTrue(
		TEXT("Android production mode rejects Google's sample rewarded ID"),
		Settings->ResolveRewardedAdUnitId(
			EOpenMobileAdsPlatform::Android,
			false
		).IsEmpty()
	);
	TestTrue(
		TEXT("iOS production mode rejects Google's sample rewarded ID"),
		Settings->ResolveRewardedAdUnitId(
			EOpenMobileAdsPlatform::IOS,
			false
		).IsEmpty()
	);
	TestTrue(
		TEXT("Production mode rejects Google's sample interstitial IDs"),
		Settings->ResolveInterstitialAdUnitId(
			EOpenMobileAdsPlatform::Android,
			false
		).IsEmpty()
		&& Settings->ResolveInterstitialAdUnitId(
			EOpenMobileAdsPlatform::IOS,
			false
		).IsEmpty()
	);
	TestTrue(
		TEXT("Production mode rejects Google's sample fixed-banner IDs"),
		Settings->ResolveBannerAdUnitId(
			EOpenMobileAdsPlatform::Android,
			false
		).IsEmpty()
		&& Settings->ResolveBannerAdUnitId(
			EOpenMobileAdsPlatform::IOS,
			false
		).IsEmpty()
	);
	Settings->AndroidRewardedAdUnitId = TEXT("ca-app-pub-1234567890123456/1234567890");
	TestEqual(
		TEXT("Production mode keeps the configured Android ID"),
		Settings->ResolveRewardedAdUnitId(EOpenMobileAdsPlatform::Android, false),
		Settings->AndroidRewardedAdUnitId
	);
	Settings->AndroidRewardedAdUnitId = TEXT("ca-app-pub-3940256099942544/5224354917");
	TestTrue(
		TEXT("Shipping validation rejects Google's sample IDs"),
		FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings, true).Contains(
			TEXT("Google sample IDs are not allowed in shipping builds.")
		)
	);
	Settings->AndroidBannerAdUnitId =
		TEXT("ca-app-pub-3940256099942544/6300978111");
	TestTrue(
		TEXT("Shipping validation rejects sample IDs for other ad formats"),
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

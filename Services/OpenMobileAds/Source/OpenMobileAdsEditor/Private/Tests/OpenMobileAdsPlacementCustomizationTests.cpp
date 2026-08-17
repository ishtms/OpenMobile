#include "OpenMobileAdsPlacementCustomization.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileAdsConfiguration.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPlacementFieldVisibilityTest,
	"OpenMobile.Ads.Editor.PlacementFieldVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPlacementFieldVisibilityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FName AppOpenPolicy = GET_MEMBER_NAME_CHECKED(
		FOpenMobileAdsPlacementSettings,
		AppOpenPolicy
	);
	const FName BannerLayout = GET_MEMBER_NAME_CHECKED(
		FOpenMobileAdsPlacementSettings,
		BannerLayout
	);
	const FName ServerVerification = GET_MEMBER_NAME_CHECKED(
		FOpenMobileAdsPlacementSettings,
		ServerVerification
	);
	const FName FallbackRewardType = GET_MEMBER_NAME_CHECKED(
		FOpenMobileAdsPlacementSettings,
		FallbackRewardType
	);
	const FName HideCachePolicy = GET_MEMBER_NAME_CHECKED(
		FOpenMobileAdsPlacementSettings,
		HideCachePolicy
	);

	TestTrue(TEXT("App Open shows its policy"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::AppOpen,
			AppOpenPolicy
		));
	TestFalse(TEXT("Rewarded hides App Open policy"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::Rewarded,
			AppOpenPolicy
		));
	TestFalse(TEXT("Rewarded hides the platform App Open override"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::Rewarded,
			GET_MEMBER_NAME_CHECKED(
				FOpenMobileAdsPlatformPlacementOverride,
				bOverrideAppOpenPolicy
			)
		));
	TestTrue(TEXT("Banner shows layout"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::Banner,
			BannerLayout
		));
	TestFalse(TEXT("Interstitial hides banner layout"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::Interstitial,
			BannerLayout
		));
	TestFalse(TEXT("Interstitial hides the platform banner override"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::Interstitial,
			GET_MEMBER_NAME_CHECKED(
				FOpenMobileAdsPlatformPlacementOverride,
				bOverrideBannerLayout
			)
		));
	TestTrue(TEXT("Rewarded shows server verification"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::Rewarded,
			ServerVerification
		));
	TestTrue(TEXT("Rewarded interstitial shows reward fallbacks"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::RewardedInterstitial,
			FallbackRewardType
		));
	TestFalse(TEXT("Interstitial hides reward settings"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::Interstitial,
			ServerVerification
		));
	TestTrue(TEXT("Persistent formats show hide cache policy"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::MediumRectangle,
			HideCachePolicy
		));
	TestFalse(TEXT("Fullscreen formats hide hide cache policy"),
		FOpenMobileAdsPlacementFieldVisibility::IsVisible(
			EOpenMobileAdFormat::Rewarded,
			HideCachePolicy
		));
	return true;
}

#endif

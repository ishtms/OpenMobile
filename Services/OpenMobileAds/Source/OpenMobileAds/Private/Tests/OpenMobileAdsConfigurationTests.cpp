#include "OpenMobileAdsConfiguration.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace OpenMobileAdsConfigurationTests
{
	bool HasIssue(
		const TArray<FOpenMobileAdsConfigurationIssue>& Issues,
		EOpenMobileAdsConfigurationIssueCode Code
	)
	{
		return Issues.ContainsByPredicate(
			[Code](const FOpenMobileAdsConfigurationIssue& Issue)
			{
				return Issue.Code == Code;
			}
		);
	}

	FOpenMobileAdsPlacementSettings MakeRewardedPlacement(
		FName Name,
		const TCHAR* AndroidId,
		const TCHAR* IOSId
	)
	{
		FOpenMobileAdsPlacementSettings Placement;
		Placement.Placement = Name;
		Placement.Format = EOpenMobileAdFormat::Rewarded;
		Placement.Android.AdUnitId = AndroidId;
		Placement.IOS.AdUnitId = IOSId;
		return Placement;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPlacementDefaultsTest,
	"OpenMobile.Ads.Configuration.DefaultsAndOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPlacementDefaultsTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdsPlacementSettings Placement =
		OpenMobileAdsConfigurationTests::MakeRewardedPlacement(
			TEXT("ContinueReward"),
			TEXT("android-unit"),
			TEXT("ios-unit")
		);
	Placement.bPreload = true;
	Placement.CooldownSeconds = 30.0;
	Placement.Android.bOverridePreload = true;
	Placement.Android.bPreload = false;

	const FOpenMobileAdsResolvedPlacement Android =
		Placement.Resolve(EOpenMobileAdsPlatform::Android);
	const FOpenMobileAdsResolvedPlacement IOS =
		Placement.Resolve(EOpenMobileAdsPlatform::IOS);

	TestEqual(TEXT("Placement key is stable"), Android.Placement, FName(TEXT("ContinueReward")));
	TestEqual(TEXT("Android ID is selected"), Android.AdUnitId, FString(TEXT("android-unit")));
	TestFalse(TEXT("Android preload override is applied"), Android.bPreload);
	TestTrue(TEXT("iOS keeps the shared preload value"), IOS.bPreload);
	TestEqual(TEXT("Shared cooldown is retained"), IOS.CooldownSeconds, 30.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPlacementValidationTest,
	"OpenMobile.Ads.Configuration.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPlacementValidationTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsConfigurationTests;

	TArray<FOpenMobileAdsPlacementSettings> Placements;
	Placements.Add(MakeRewardedPlacement(
		TEXT("ContinueReward"),
		TEXT("android-reward"),
		TEXT("ios-reward")
	));
	Placements.Add(MakeRewardedPlacement(
		TEXT("continuereward"),
		TEXT("android-reward-2"),
		TEXT("ios-reward-2")
	));
	Placements.Add(MakeRewardedPlacement(
		NAME_None,
		TEXT("android-empty-name"),
		TEXT("ios-empty-name")
	));
	Placements.Add(MakeRewardedPlacement(
		TEXT("DuplicateAndroidId"),
		TEXT("android-reward"),
		TEXT("ios-other")
	));

	FOpenMobileAdsPlacementSettings InvalidRefresh = MakeRewardedPlacement(
		TEXT("InvalidRefresh"),
		TEXT("android-refresh"),
		TEXT("ios-refresh")
	);
	InvalidRefresh.RefreshIntervalSeconds = 10.0;
	Placements.Add(InvalidRefresh);

	FOpenMobileAdsPlacementSettings InvalidCap = MakeRewardedPlacement(
		TEXT("InvalidCap"),
		TEXT("android-cap"),
		TEXT("ios-cap")
	);
	InvalidCap.FrequencyCap.MaxImpressions = 1;
	Placements.Add(InvalidCap);

	const TArray<FOpenMobileAdsConfigurationIssue> Issues =
		FOpenMobileAdsConfigurationValidator::Validate(Placements);

	TestTrue(TEXT("Empty names are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::EmptyPlacement));
	TestTrue(TEXT("Case conflicts are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::CaseConflict));
	TestTrue(TEXT("Duplicate Android IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::DuplicateAndroidAdUnitId));
	TestTrue(TEXT("Rewarded refresh is rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::RefreshNotSupported));
	TestTrue(TEXT("Incomplete caps are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::InvalidFrequencyCap));
	return true;
}

#endif

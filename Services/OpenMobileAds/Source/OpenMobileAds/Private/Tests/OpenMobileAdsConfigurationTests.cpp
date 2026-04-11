#include "OpenMobileAdsConfiguration.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

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
	Placement.bEnabled = false;
	Placement.CooldownSeconds = 30.0;
	Placement.FrequencyCap.MaxImpressions = 2;
	Placement.FrequencyCap.WindowSeconds = 60.0;
	Placement.ProviderOptions.Add(TEXT("SharedOption"), TEXT("shared"));
	Placement.Android.bOverridePreload = true;
	Placement.Android.bPreload = false;
	Placement.Android.bOverrideEnabled = true;
	Placement.Android.bEnabled = true;
	Placement.Android.bOverrideCooldown = true;
	Placement.Android.CooldownSeconds = 10.0;
	Placement.Android.ProviderOptions.Add(TEXT("SharedOption"), TEXT("android"));

	const FOpenMobileAdsResolvedPlacement Android =
		Placement.Resolve(EOpenMobileAdsPlatform::Android);
	const FOpenMobileAdsResolvedPlacement IOS =
		Placement.Resolve(EOpenMobileAdsPlatform::IOS);

	TestEqual(TEXT("Placement key is stable"), Android.Placement, FName(TEXT("ContinueReward")));
	TestEqual(TEXT("Android ID is selected"), Android.AdUnitId, FString(TEXT("android-unit")));
	TestTrue(TEXT("Android enabled override is applied"), Android.bEnabled);
	TestFalse(TEXT("Android preload override is applied"), Android.bPreload);
	TestFalse(TEXT("iOS keeps the shared enabled value"), IOS.bEnabled);
	TestTrue(TEXT("iOS keeps the shared preload value"), IOS.bPreload);
	TestEqual(TEXT("Shared cooldown is retained"), IOS.CooldownSeconds, 30.0);
	TestEqual(TEXT("Android cooldown override is applied"), Android.CooldownSeconds, 10.0);
	TestEqual(TEXT("Frequency cap count is retained"), IOS.FrequencyCap.MaxImpressions, 2);
	TestEqual(TEXT("Frequency cap window is retained"), IOS.FrequencyCap.WindowSeconds, 60.0);
	TestEqual(TEXT("Platform provider options override shared values"), Android.ProviderOptions[TEXT("SharedOption")], FString(TEXT("android")));
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
	Placements.Add(MakeRewardedPlacement(
		TEXT("DuplicateIOSId"),
		TEXT("android-other"),
		TEXT("ios-reward")
	));
	Placements.Add(MakeRewardedPlacement(
		TEXT("MissingIds"),
		TEXT(""),
		TEXT("")
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

	FOpenMobileAdsPlacementSettings InvalidCooldown = MakeRewardedPlacement(
		TEXT("InvalidCooldown"),
		TEXT("android-cooldown"),
		TEXT("ios-cooldown")
	);
	InvalidCooldown.CooldownSeconds = -1.0;
	Placements.Add(InvalidCooldown);

	FOpenMobileAdsPlacementSettings InvalidOption = MakeRewardedPlacement(
		TEXT("InvalidOption"),
		TEXT("android-option"),
		TEXT("ios-option")
	);
	InvalidOption.ProviderOptions.Add(NAME_None, TEXT("value"));
	Placements.Add(InvalidOption);

	const TArray<FOpenMobileAdsConfigurationIssue> Issues =
		FOpenMobileAdsConfigurationValidator::Validate(Placements);

	TestTrue(TEXT("Empty names are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::EmptyPlacement));
	TestTrue(TEXT("Case conflicts are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::CaseConflict));
	TestTrue(TEXT("Duplicate Android IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::DuplicateAndroidAdUnitId));
	TestTrue(TEXT("Duplicate iOS IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::DuplicateIOSAdUnitId));
	TestTrue(TEXT("Missing Android IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::MissingAndroidAdUnitId));
	TestTrue(TEXT("Missing iOS IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::MissingIOSAdUnitId));
	TestTrue(TEXT("Rewarded refresh is rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::RefreshNotSupported));
	TestTrue(TEXT("Incomplete caps are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::InvalidFrequencyCap));
	TestTrue(TEXT("Negative cooldowns are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::InvalidCooldown));
	TestTrue(TEXT("Empty provider option names are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::EmptyProviderOption));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPlacementConfigLoadingTest,
	"OpenMobile.Ads.Configuration.ConfigLoading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPlacementConfigLoadingTest::RunTest(const FString& Parameters)
{
	const FString ConfigPath = FPaths::CreateTempFilename(
		*FPaths::ProjectIntermediateDir(),
		TEXT("OpenMobileAdsConfig"),
		TEXT(".ini")
	);
	const FString Config = TEXT(
		"[/Script/OpenMobileAds.OpenMobileAdsSettings]\n"
		"PreferredProvider=ConfiguredAds\n"
		"Placements=(Placement=ConfiguredReward,Format=Rewarded,bEnabled=True,bPreload=True,RefreshIntervalSeconds=0.000000,FrequencyCap=(MaxImpressions=2,WindowSeconds=60.000000),CooldownSeconds=30.000000,Android=(AdUnitId=\"android-config\",bOverridePreload=True,bPreload=False),IOS=(AdUnitId=\"ios-config\"))\n"
	);
	if (!FFileHelper::SaveStringToFile(Config, *ConfigPath))
	{
		AddError(TEXT("Could not create the temporary ads config."));
		return false;
	}

	UOpenMobileAdsSettings* Settings = NewObject<UOpenMobileAdsSettings>();
	Settings->LoadConfig(UOpenMobileAdsSettings::StaticClass(), *ConfigPath);
	IFileManager::Get().Delete(*ConfigPath, false, true, true);

	TestEqual(TEXT("Preferred provider loads from config"), Settings->PreferredProvider, FName(TEXT("ConfiguredAds")));
	TestEqual(TEXT("One placement loads from config"), Settings->Placements.Num(), 1);
	if (Settings->Placements.Num() == 1)
	{
		const FOpenMobileAdsPlacementSettings& Placement = Settings->Placements[0];
		TestEqual(TEXT("Placement key loads from config"), Placement.Placement, FName(TEXT("ConfiguredReward")));
		TestTrue(TEXT("Shared preload loads from config"), Placement.bPreload);
		TestEqual(TEXT("Frequency cap count loads from config"), Placement.FrequencyCap.MaxImpressions, 2);
		TestEqual(TEXT("Frequency cap window loads from config"), Placement.FrequencyCap.WindowSeconds, 60.0);
		TestEqual(TEXT("Cooldown loads from config"), Placement.CooldownSeconds, 30.0);
		TestEqual(TEXT("Android ID loads from config"), Placement.Android.AdUnitId, FString(TEXT("android-config")));
		TestTrue(TEXT("Android override flag loads from config"), Placement.Android.bOverridePreload);
		TestFalse(TEXT("Android override value loads from config"), Placement.Android.bPreload);
		TestEqual(TEXT("iOS ID loads from config"), Placement.IOS.AdUnitId, FString(TEXT("ios-config")));
	}
	return true;
}

#endif

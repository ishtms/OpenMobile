#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsCapabilities.h"

#if WITH_DEV_AUTOMATION_TESTS

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
		TEXT("ContinueReward"),
		TEXT("android-reward-exact-duplicate"),
		TEXT("ios-reward-exact-duplicate")
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

	FOpenMobileAdsPlacementSettings NegativeRefresh = MakeRewardedPlacement(
		TEXT("NegativeRefresh"),
		TEXT("android-negative-refresh"),
		TEXT("ios-negative-refresh")
	);
	NegativeRefresh.RefreshIntervalSeconds = -1.0;
	Placements.Add(NegativeRefresh);

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

	FOpenMobileAdsPlacementSettings InvalidFallbackRewardAmount = MakeRewardedPlacement(
		TEXT("InvalidFallbackRewardAmount"),
		TEXT("android-reward-amount"),
		TEXT("ios-reward-amount")
	);
	InvalidFallbackRewardAmount.FallbackRewardAmount = -1;
	Placements.Add(InvalidFallbackRewardAmount);

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
	TestTrue(TEXT("Exact duplicate names are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::DuplicatePlacement));
	TestTrue(TEXT("Case conflicts are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::CaseConflict));
	TestTrue(TEXT("Duplicate Android IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::DuplicateAndroidAdUnitId));
	TestTrue(TEXT("Duplicate iOS IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::DuplicateIOSAdUnitId));
	TestTrue(TEXT("Missing Android IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::MissingAndroidAdUnitId));
	TestTrue(TEXT("Missing iOS IDs are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::MissingIOSAdUnitId));
	TestTrue(TEXT("Rewarded refresh is rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::RefreshNotSupported));
	TestTrue(TEXT("Negative refresh intervals are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::InvalidRefreshInterval));
	TestTrue(TEXT("Incomplete caps are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::InvalidFrequencyCap));
	TestTrue(TEXT("Negative cooldowns are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::InvalidCooldown));
	TestTrue(TEXT("Negative reward amount fallbacks are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::InvalidFallbackRewardAmount));
	TestTrue(TEXT("Empty provider option names are rejected"), HasIssue(Issues, EOpenMobileAdsConfigurationIssueCode::EmptyProviderOption));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPlacementCapabilityValidationTest,
	"OpenMobile.Ads.Configuration.ProviderCapabilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPlacementCapabilityValidationTest::RunTest(const FString& Parameters)
{
	FOpenMobileAdsPlacementSettings Placement =
		OpenMobileAdsConfigurationTests::MakeRewardedPlacement(
			TEXT("UnsupportedInterstitial"),
			TEXT("android-interstitial"),
			TEXT("ios-interstitial")
		);
	Placement.Format = EOpenMobileAdFormat::Interstitial;
	const FOpenMobileAdsPlacementSettings UnsupportedOperation =
		OpenMobileAdsConfigurationTests::MakeRewardedPlacement(
			TEXT("UnsupportedRewardedOperation"),
			TEXT("android-rewarded-operation"),
			TEXT("ios-rewarded-operation")
		);

	FOpenMobileAdFormatCapabilities Rewarded;
	Rewarded.Format = EOpenMobileAdFormat::Rewarded;
	Rewarded.bCanLoad = true;
	FOpenMobileAdsProviderCapabilities Capabilities;
	Capabilities.Provider = TEXT("RewardedOnlyAds");
	Capabilities.Formats.Add(Rewarded);

	const TArray<FOpenMobileAdsConfigurationIssue> Issues =
		FOpenMobileAdsConfigurationValidator::ValidateProviderCapabilities(
			{Placement, UnsupportedOperation},
			Capabilities
		);
	TestTrue(
		TEXT("Unsupported provider formats are rejected"),
		OpenMobileAdsConfigurationTests::HasIssue(
			Issues,
			EOpenMobileAdsConfigurationIssueCode::UnsupportedProviderFormat
		)
	);
	TestTrue(
		TEXT("Unsupported provider operations are rejected"),
		OpenMobileAdsConfigurationTests::HasIssue(
			Issues,
			EOpenMobileAdsConfigurationIssueCode::UnsupportedProviderOperation
		)
	);
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
	UOpenMobileAdsSettings* SavedSettings = NewObject<UOpenMobileAdsSettings>();
	SavedSettings->PreferredProvider = TEXT("ConfiguredAds");
	SavedSettings->bDevelopmentTestMode = true;
	SavedSettings->TestDeviceIdentifiers.Add(TEXT("GLOBAL-TEST-DEVICE"));
	SavedSettings->DebugGeography =
		EOpenMobileAdsDebugGeography::RegulatedUsState;
	SavedSettings->bEnableTrackingAuthorization = true;
	SavedSettings->TrackingUsageDescription =
		TEXT("We use this permission to measure advertising performance.");
	SavedSettings->bDelayAdsInitializationUntilTrackingAuthorization = false;
	SavedSettings->RetryPolicy.MaxRetryAttempts = 4;
	SavedSettings->RetryPolicy.InitialDelaySeconds = 0.5;
	SavedSettings->RetryPolicy.BackoffMultiplier = 3.0;
	SavedSettings->RetryPolicy.MaxDelaySeconds = 12.0;
	SavedSettings->RetryPolicy.bUseJitter = false;
	SavedSettings->NoFillRetryPolicy.MaxRetryAttempts = 1;
	SavedSettings->NoFillRetryPolicy.InitialDelaySeconds = 45.0;
	SavedSettings->NoFillRetryPolicy.BackoffMultiplier = 2.0;
	SavedSettings->NoFillRetryPolicy.MaxDelaySeconds = 180.0;
	SavedSettings->NoFillRetryPolicy.bUseJitter = true;
	SavedSettings->Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::Yes;
	SavedSettings->Privacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::No;
	SavedSettings->Privacy.bDelayProviderInitializationUntilConsent = false;
	SavedSettings->RequestConfiguration.MaxAdContentRating =
		EOpenMobileAdsMaxAdContentRating::Teen;
	SavedSettings->ConvenienceRewardedPlacement = TEXT("ConfiguredReward");
	FOpenMobileAdsPlacementSettings Placement =
		OpenMobileAdsConfigurationTests::MakeRewardedPlacement(
			TEXT("ConfiguredReward"),
			TEXT("android-config"),
			TEXT("ios-config")
		);
	Placement.bPreload = true;
	Placement.CooldownSeconds = 30.0;
	Placement.FallbackRewardType = TEXT("gold-token");
	Placement.FallbackRewardAmount = 25;
	Placement.FrequencyCap.MaxImpressions = 2;
	Placement.FrequencyCap.WindowSeconds = 60.0;
	Placement.Android.bOverridePreload = true;
	Placement.Android.bPreload = false;
	SavedSettings->Placements.Add(Placement);
	SavedSettings->SaveConfig(CPF_Config, *ConfigPath, GConfig, false);

	UOpenMobileAdsSettings* SettingsAfterRestart = NewObject<UOpenMobileAdsSettings>();
	SettingsAfterRestart->LoadConfig(UOpenMobileAdsSettings::StaticClass(), *ConfigPath);
	IFileManager::Get().Delete(*ConfigPath, false, true, true);

	TestEqual(TEXT("Preferred provider survives restart"), SettingsAfterRestart->PreferredProvider, FName(TEXT("ConfiguredAds")));
	TestTrue(TEXT("Development test mode survives restart"), SettingsAfterRestart->bDevelopmentTestMode);
	TestEqual(
		TEXT("Global test-device identifiers survive restart"),
		SettingsAfterRestart->TestDeviceIdentifiers,
		TArray<FString>({TEXT("GLOBAL-TEST-DEVICE")})
	);
	TestEqual(
		TEXT("Consent debug geography survives restart"),
		SettingsAfterRestart->DebugGeography,
		EOpenMobileAdsDebugGeography::RegulatedUsState
	);
	TestTrue(
		TEXT("ATT opt-in survives restart"),
		SettingsAfterRestart->bEnableTrackingAuthorization
	);
	TestEqual(
		TEXT("ATT usage description survives restart"),
		SettingsAfterRestart->TrackingUsageDescription,
		FString(TEXT("We use this permission to measure advertising performance."))
	);
	TestFalse(
		TEXT("ATT initialization policy survives restart"),
		SettingsAfterRestart->bDelayAdsInitializationUntilTrackingAuthorization
	);
	TestEqual(TEXT("Retry count survives restart"), SettingsAfterRestart->RetryPolicy.MaxRetryAttempts, 4);
	TestEqual(TEXT("Initial retry delay survives restart"), SettingsAfterRestart->RetryPolicy.InitialDelaySeconds, 0.5);
	TestEqual(TEXT("Retry backoff survives restart"), SettingsAfterRestart->RetryPolicy.BackoffMultiplier, 3.0);
	TestEqual(TEXT("Maximum retry delay survives restart"), SettingsAfterRestart->RetryPolicy.MaxDelaySeconds, 12.0);
	TestFalse(TEXT("Retry jitter survives restart"), SettingsAfterRestart->RetryPolicy.bUseJitter);
	TestEqual(TEXT("No-fill retry count survives restart"), SettingsAfterRestart->NoFillRetryPolicy.MaxRetryAttempts, 1);
	TestEqual(TEXT("No-fill retry delay survives restart"), SettingsAfterRestart->NoFillRetryPolicy.InitialDelaySeconds, 45.0);
	TestEqual(TEXT("No-fill retry backoff survives restart"), SettingsAfterRestart->NoFillRetryPolicy.BackoffMultiplier, 2.0);
	TestEqual(TEXT("No-fill retry cap survives restart"), SettingsAfterRestart->NoFillRetryPolicy.MaxDelaySeconds, 180.0);
	TestTrue(TEXT("No-fill retry jitter survives restart"), SettingsAfterRestart->NoFillRetryPolicy.bUseJitter);
	TestEqual(
		TEXT("Child-directed setting survives restart"),
		SettingsAfterRestart->Privacy.ChildDirectedTreatment,
		EOpenMobileAdsAgeTreatment::Yes
	);
	TestEqual(
		TEXT("Under-age setting survives restart"),
		SettingsAfterRestart->Privacy.UnderAgeOfConsent,
		EOpenMobileAdsAgeTreatment::No
	);
	TestFalse(
		TEXT("Consent initialization policy survives restart"),
		SettingsAfterRestart->Privacy.bDelayProviderInitializationUntilConsent
	);
	TestEqual(
		TEXT("Maximum ad content rating survives restart"),
		SettingsAfterRestart->RequestConfiguration.MaxAdContentRating,
		EOpenMobileAdsMaxAdContentRating::Teen
	);
	TestEqual(
		TEXT("Convenience rewarded placement survives restart"),
		SettingsAfterRestart->ConvenienceRewardedPlacement,
		FName(TEXT("ConfiguredReward"))
	);
	TestEqual(TEXT("One placement survives restart"), SettingsAfterRestart->Placements.Num(), 1);
	if (SettingsAfterRestart->Placements.Num() == 1)
	{
		const FOpenMobileAdsPlacementSettings& LoadedPlacement =
			SettingsAfterRestart->Placements[0];
		TestEqual(TEXT("Placement key survives restart"), LoadedPlacement.Placement, FName(TEXT("ConfiguredReward")));
		TestTrue(TEXT("Shared preload survives restart"), LoadedPlacement.bPreload);
		TestEqual(TEXT("Frequency cap count survives restart"), LoadedPlacement.FrequencyCap.MaxImpressions, 2);
		TestEqual(TEXT("Frequency cap window survives restart"), LoadedPlacement.FrequencyCap.WindowSeconds, 60.0);
		TestEqual(TEXT("Cooldown survives restart"), LoadedPlacement.CooldownSeconds, 30.0);
		TestEqual(TEXT("Reward type fallback survives restart"), LoadedPlacement.FallbackRewardType, FString(TEXT("gold-token")));
		TestEqual(TEXT("Reward amount fallback survives restart"), LoadedPlacement.FallbackRewardAmount, static_cast<int64>(25));
		TestEqual(TEXT("Android ID survives restart"), LoadedPlacement.Android.AdUnitId, FString(TEXT("android-config")));
		TestTrue(TEXT("Android override flag survives restart"), LoadedPlacement.Android.bOverridePreload);
		TestFalse(TEXT("Android override value survives restart"), LoadedPlacement.Android.bPreload);
		TestEqual(TEXT("iOS ID survives restart"), LoadedPlacement.IOS.AdUnitId, FString(TEXT("ios-config")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProjectSettingsValidationTest,
	"OpenMobile.Ads.Configuration.ProjectSettingsValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProjectSettingsValidationTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsConfigurationTests;
	UOpenMobileAdsSettings* Settings = NewObject<UOpenMobileAdsSettings>();
	Settings->Placements.Add(MakeRewardedPlacement(
		TEXT("ContinueReward"),
		TEXT("android-reward"),
		TEXT("ios-reward")
	));

	TestFalse(TEXT("Development test mode is off by default"), Settings->bDevelopmentTestMode);
	TestEqual(
		TEXT("Consent debug geography is disabled by default"),
		Settings->DebugGeography,
		EOpenMobileAdsDebugGeography::Disabled
	);
	TestFalse(
		TEXT("ATT requests are disabled by default"),
		Settings->bEnableTrackingAuthorization
	);
	TestTrue(
		TEXT("ATT defaults to delaying ads initialization when enabled"),
		Settings->bDelayAdsInitializationUntilTrackingAuthorization
	);
	TestFalse(
		TEXT("An unset development mode stays off outside Shipping"),
		UOpenMobileAdsSettings::ResolveDevelopmentTestMode(false, false)
	);
	TestTrue(
		TEXT("An enabled development mode runs outside Shipping"),
		UOpenMobileAdsSettings::ResolveDevelopmentTestMode(true, false)
	);
	TestFalse(
		TEXT("Shipping always forces production behavior"),
		UOpenMobileAdsSettings::ResolveDevelopmentTestMode(true, true)
	);
	TestEqual(TEXT("Default retry count is bounded"), Settings->RetryPolicy.MaxRetryAttempts, 2);
	TestTrue(TEXT("Default retry policy is valid"), Settings->RetryPolicy.IsValid());
	TestTrue(TEXT("Default no-fill retry policy is valid"), Settings->NoFillRetryPolicy.IsValid());
	TestTrue(
		TEXT("No-fill retries default to a slower initial delay"),
		Settings->NoFillRetryPolicy.InitialDelaySeconds
			> Settings->RetryPolicy.InitialDelaySeconds
	);
	TestEqual(
		TEXT("No-fill errors select their dedicated retry policy"),
		Settings->GetRetryPolicyForError(EOpenMobileAdsErrorCode::NoFill)
			.InitialDelaySeconds,
		Settings->NoFillRetryPolicy.InitialDelaySeconds
	);
	TestEqual(
		TEXT("Other errors keep the general retry policy"),
		Settings->GetRetryPolicyForError(EOpenMobileAdsErrorCode::NativeFailure)
			.InitialDelaySeconds,
		Settings->RetryPolicy.InitialDelaySeconds
	);
	TestTrue(
		TEXT("Default project settings are valid"),
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false).IsEmpty()
	);

	Settings->ConvenienceRewardedPlacement = TEXT("MissingReward");
	const TArray<FOpenMobileAdsConfigurationIssue> ConvenienceIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false);
	TestTrue(
		TEXT("Missing convenience rewarded placements are rejected"),
		HasIssue(
			ConvenienceIssues,
			EOpenMobileAdsConfigurationIssueCode::InvalidConvenienceRewardedPlacement
		)
	);
	Settings->ConvenienceRewardedPlacement = TEXT("ContinueReward");
	Settings->Placements[0].Format = EOpenMobileAdFormat::Interstitial;
	TestTrue(
		TEXT("Non-rewarded convenience placements are rejected"),
		HasIssue(
			FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false),
			EOpenMobileAdsConfigurationIssueCode::InvalidConvenienceRewardedPlacement
		)
	);
	Settings->Placements[0].Format = EOpenMobileAdFormat::Rewarded;
	Settings->Placements[0].bEnabled = false;
	TestTrue(
		TEXT("Disabled convenience rewarded placements are rejected"),
		HasIssue(
			FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false),
			EOpenMobileAdsConfigurationIssueCode::InvalidConvenienceRewardedPlacement
		)
	);
	Settings->Placements[0].bEnabled = true;
	TestFalse(
		TEXT("An enabled rewarded convenience placement is valid"),
		HasIssue(
			FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false),
			EOpenMobileAdsConfigurationIssueCode::InvalidConvenienceRewardedPlacement
		)
	);

	Settings->RetryPolicy.MaxRetryAttempts = -1;
	Settings->RetryPolicy.InitialDelaySeconds = -1.0;
	const TArray<FOpenMobileAdsConfigurationIssue> RetryIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false);
	TestTrue(
		TEXT("Invalid retry policies are rejected"),
		HasIssue(RetryIssues, EOpenMobileAdsConfigurationIssueCode::InvalidRetryPolicy)
	);

	Settings->RetryPolicy = FOpenMobileAdsRetryPolicy();
	Settings->NoFillRetryPolicy.MaxRetryAttempts = -1;
	const TArray<FOpenMobileAdsConfigurationIssue> NoFillRetryIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false);
	TestTrue(
		TEXT("Invalid no-fill retry policies are rejected"),
		HasIssue(
			NoFillRetryIssues,
			EOpenMobileAdsConfigurationIssueCode::InvalidRetryPolicy
		)
	);
	Settings->NoFillRetryPolicy = FOpenMobileAdsRetryPolicy();
	Settings->bDevelopmentTestMode = true;
	const TArray<FOpenMobileAdsConfigurationIssue> ShippingIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, true);
	TestTrue(
		TEXT("Development test mode is rejected for shipping"),
		HasIssue(ShippingIssues, EOpenMobileAdsConfigurationIssueCode::UnsafeShippingTestMode)
	);

	Settings->bDevelopmentTestMode = false;
	Settings->TestDeviceIdentifiers = {TEXT("invalid device")};
	const TArray<FOpenMobileAdsConfigurationIssue> InvalidDeviceIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false);
	TestTrue(
		TEXT("Malformed global test-device identifiers are rejected"),
		HasIssue(
			InvalidDeviceIssues,
			EOpenMobileAdsConfigurationIssueCode::InvalidTestDeviceIdentifier
		)
	);

	Settings->TestDeviceIdentifiers = {TEXT("GLOBAL-TEST-DEVICE")};
	const TArray<FOpenMobileAdsConfigurationIssue> ShippingDeviceIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, true);
	TestTrue(
		TEXT("Global test-device identifiers are rejected for shipping"),
		HasIssue(
			ShippingDeviceIssues,
			EOpenMobileAdsConfigurationIssueCode::UnsafeShippingTestDeviceIdentifier
		)
	);

	Settings->DebugGeography = EOpenMobileAdsDebugGeography::Eea;
	const TArray<FOpenMobileAdsConfigurationIssue> ShippingGeographyIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, true);
	TestTrue(
		TEXT("Enabled debug geography is rejected for shipping"),
		HasIssue(
			ShippingGeographyIssues,
			EOpenMobileAdsConfigurationIssueCode::UnsafeShippingDebugGeography
		)
	);

	Settings->TrackingUsageDescription.Reset();
	const TArray<FOpenMobileAdsConfigurationIssue> DisabledTrackingTextIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false);
	TestFalse(
		TEXT("Disabled ATT does not require a usage description"),
		HasIssue(
			DisabledTrackingTextIssues,
			EOpenMobileAdsConfigurationIssueCode::InvalidTrackingUsageDescription
		)
	);
	Settings->bEnableTrackingAuthorization = true;
	const TArray<FOpenMobileAdsConfigurationIssue> MissingTrackingTextIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false);
	TestTrue(
		TEXT("Enabled ATT requires a usage description"),
		HasIssue(
			MissingTrackingTextIssues,
			EOpenMobileAdsConfigurationIssueCode::InvalidTrackingUsageDescription
		)
	);
	Settings->TrackingUsageDescription =
		TEXT("We use this permission to measure advertising performance.");
	const TArray<FOpenMobileAdsConfigurationIssue> ValidTrackingTextIssues =
		FOpenMobileAdsConfigurationValidator::ValidateSettings(*Settings, false);
	TestFalse(
		TEXT("A configured ATT usage description passes validation"),
		HasIssue(
			ValidTrackingTextIssues,
			EOpenMobileAdsConfigurationIssueCode::InvalidTrackingUsageDescription
		)
	);
	return true;
}

#endif

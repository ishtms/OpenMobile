#if WITH_DEV_AUTOMATION_TESTS

#include "Features/IModularFeatures.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenMobileDeviceBuildValidation.h"

namespace
{
const FOpenMobileDeviceBuildValidationIssue* FindIssue(
	const FOpenMobileDeviceBuildValidationReport& Report,
	const FName Code
)
{
	return Report.Issues.FindByPredicate(
		[Code](const FOpenMobileDeviceBuildValidationIssue& Issue)
		{
			return Issue.Code == Code;
		}
	);
}

FString GetFixturePath(const TCHAR* RelativePath)
{
	const TSharedPtr<IPlugin> Plugin =
		IPluginManager::Get().FindPlugin(TEXT("OpenMobileDevice"));
	return Plugin.IsValid()
		? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Tests/Fixtures/BuildValidation"), RelativePath)
		: FString();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceBuildValidationContractTest,
	"OpenMobile.Device.BuildValidation.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceBuildValidationContractTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FName FeatureName =
		IOpenMobileDeviceBuildValidationContributor::GetModularFeatureName();
	TestEqual(
		TEXT("Contributor uses the Store Doctor discovery contract"),
		FeatureName,
		FName(TEXT("OpenMobile.StoreDoctor.ValidationContributor"))
	);
	TestTrue(
		TEXT("Device validation contributor is registered"),
		IModularFeatures::Get().IsModularFeatureAvailable(FeatureName)
	);
	if (!IModularFeatures::Get().IsModularFeatureAvailable(FeatureName))
	{
		return false;
	}

	IOpenMobileDeviceBuildValidationContributor& Contributor =
		IModularFeatures::Get()
			.GetModularFeature<IOpenMobileDeviceBuildValidationContributor>(
				FeatureName
			);
	const FOpenMobileDeviceBuildValidationInput Input =
		Contributor.CaptureProjectInput();
	TestEqual(
		TEXT("Settings category is captured"),
		Input.SettingsCategory,
		FName(TEXT("OpenMobile"))
	);
	TestEqual(
		TEXT("Settings section is captured"),
		Input.SettingsSection,
		FName(TEXT("OpenMobile Device"))
	);
	TestEqual(
		TEXT("Settings display name is captured"),
		Input.SettingsDisplayName,
		FString(TEXT("OpenMobile Device"))
	);
	TestEqual(
		TEXT("Settings stage through Game config"),
		Input.SettingsConfigName,
		FName(TEXT("Game"))
	);
	TestTrue(
		TEXT("Every Device setting is staged"),
		Input.bAllSettingsPropertiesStaged
	);

	const FOpenMobileDeviceBuildValidationPlatform* Android =
		Input.Platforms.FindByPredicate(
			[](const FOpenMobileDeviceBuildValidationPlatform& Platform)
			{
				return Platform.Name == TEXT("Android");
			}
		);
	const FOpenMobileDeviceBuildValidationPlatform* IOS =
		Input.Platforms.FindByPredicate(
			[](const FOpenMobileDeviceBuildValidationPlatform& Platform)
			{
				return Platform.Name == TEXT("IOS");
			}
		);
	TestTrue(
		TEXT("Android config hierarchy resolves"),
		Android && Android->bConfigHierarchyResolved
	);
	TestTrue(
		TEXT("iOS config hierarchy resolves"),
		IOS && IOS->bConfigHierarchyResolved
	);
	const FOpenMobileDeviceBuildValidationReport Report =
		Contributor.Validate(Input, EOpenMobileDeviceBuildValidationTarget::Shipping);
	TestFalse(
		TEXT("Repository Device configuration passes Shipping validation"),
		Report.HasBlockingIssues()
	);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceBuildValidationPlatformOverridesTest,
	"OpenMobile.Device.BuildValidation.PlatformOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceBuildValidationPlatformOverridesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FString ConfigRoot = GetFixturePath(TEXT("Config"));
	FOpenMobileDeviceBuildValidationPlatform Android;
	FOpenMobileDeviceBuildValidationPlatform IOS;
	TestTrue(
		TEXT("Android fixture hierarchy loads"),
		FOpenMobileDeviceBuildValidation::LoadPlatformSettingsFromConfigHierarchy(
			FPaths::EngineConfigDir(),
			ConfigRoot,
			TEXT("Android"),
			Android
		)
	);
	TestTrue(
		TEXT("iOS fixture hierarchy loads"),
		FOpenMobileDeviceBuildValidation::LoadPlatformSettingsFromConfigHierarchy(
			FPaths::EngineConfigDir(),
			ConfigRoot,
			TEXT("IOS"),
			IOS
		)
	);
	TestEqual(TEXT("Android override wins"), Android.FallbackPollingIntervalSeconds, 3.5f);
	TestEqual(TEXT("iOS override wins"), IOS.FallbackPollingIntervalSeconds, 4.5f);
	TestFalse(TEXT("Android threshold mode is overridden"), Android.bUsePlatformDefaultLowStorageThreshold);
	TestEqual(
		TEXT("Android threshold value is overridden"),
		Android.LowStorageThresholdBytes,
		int64(268435456)
	);
	TestEqual(
		TEXT("Common config reaches both platforms"),
		IOS.EndpointReachabilityMaximumConcurrentRequests,
		6
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceBuildValidationFixturesTest,
	"OpenMobile.Device.BuildValidation.Fixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceBuildValidationFixturesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FOpenMobileDeviceBuildValidationReport Valid =
		FOpenMobileDeviceBuildValidation::ValidateFixture(
			GetFixturePath(TEXT("valid.json")),
			EOpenMobileDeviceBuildValidationTarget::Shipping
		);
	TestEqual(TEXT("Valid fixture has no findings"), Valid.Issues.Num(), 0);

	const FOpenMobileDeviceBuildValidationReport Missing =
		FOpenMobileDeviceBuildValidation::ValidateFixture(
			GetFixturePath(TEXT("missing.json")),
			EOpenMobileDeviceBuildValidationTarget::Shipping
		);
	TestNotNull(
		TEXT("Missing fixture has a typed finding"),
		FindIssue(Missing, TEXT("Device.BuildValidation.ConfigurationMissing"))
	);
	TestTrue(TEXT("Missing fixture blocks Shipping"), Missing.HasBlockingIssues());

	const FOpenMobileDeviceBuildValidationReport Malformed =
		FOpenMobileDeviceBuildValidation::ValidateFixture(
			GetFixturePath(TEXT("malformed.json")),
			EOpenMobileDeviceBuildValidationTarget::Shipping
		);
	TestNotNull(
		TEXT("Malformed fixture has a typed finding"),
		FindIssue(Malformed, TEXT("Device.BuildValidation.ConfigurationMalformed"))
	);
	TestTrue(TEXT("Malformed fixture blocks Shipping"), Malformed.HasBlockingIssues());

	const FString ConflictingPath = GetFixturePath(TEXT("conflicting.json"));
	const FOpenMobileDeviceBuildValidationReport Development =
		FOpenMobileDeviceBuildValidation::ValidateFixture(
			ConflictingPath,
			EOpenMobileDeviceBuildValidationTarget::Development
		);
	TestTrue(TEXT("Conflicting fixture has findings"), Development.Issues.Num() > 0);
	for (const FOpenMobileDeviceBuildValidationIssue& Issue : Development.Issues)
	{
		TestEqual(
			TEXT("Development findings are warnings"),
			Issue.Severity,
			EOpenMobileDeviceBuildValidationSeverity::Warning
		);
		TestFalse(TEXT("Development findings do not block"), Issue.bBlocksBuild);
	}
	TestNotNull(
		TEXT("Conflicting platforms are identified"),
		FindIssue(
			Development,
			TEXT("Device.BuildValidation.ConflictingPlatformConfiguration")
		)
	);

	const FOpenMobileDeviceBuildValidationReport Shipping =
		FOpenMobileDeviceBuildValidation::ValidateFixture(
			ConflictingPath,
			EOpenMobileDeviceBuildValidationTarget::Shipping
		);
	TestTrue(TEXT("Conflicting fixture blocks Shipping"), Shipping.HasBlockingIssues());
	for (const FOpenMobileDeviceBuildValidationIssue& Issue : Shipping.Issues)
	{
		TestEqual(
			TEXT("Shipping findings are errors"),
			Issue.Severity,
			EOpenMobileDeviceBuildValidationSeverity::Error
		);
		TestTrue(TEXT("Shipping findings block"), Issue.bBlocksBuild);
	}

	const FOpenMobileDeviceBuildValidationReport PrivacyUnsafe =
		FOpenMobileDeviceBuildValidation::ValidateFixture(
			GetFixturePath(TEXT("privacy-unsafe.json")),
			EOpenMobileDeviceBuildValidationTarget::StoreSubmission
		);
	for (const FName RequiredCode : {
		FName(TEXT("Device.BuildValidation.PrivacyUnsafeEndpoint")),
		FName(TEXT("Device.BuildValidation.InvalidUrlScheme")),
		FName(TEXT("Device.BuildValidation.InvalidPackageName")),
		FName(TEXT("Device.BuildValidation.PrivacyUnsafePermission")),
		FName(TEXT("Device.BuildValidation.PrivacyReasonMissing"))
	})
	{
		TestNotNull(
			*FString::Printf(TEXT("Privacy fixture reports %s"), *RequiredCode.ToString()),
			FindIssue(PrivacyUnsafe, RequiredCode)
		);
	}
	TestTrue(
		TEXT("Privacy-unsafe fixture blocks store submission"),
		PrivacyUnsafe.HasBlockingIssues()
	);
	return true;
}

#endif

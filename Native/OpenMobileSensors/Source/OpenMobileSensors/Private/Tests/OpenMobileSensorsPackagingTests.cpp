#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsPackagingValidation.h"
#include "OpenMobileSensorsSettings.h"

namespace OpenMobileSensorsPackagingTestsPrivate
{
	bool HasIssue(
		const TArray<FOpenMobileSensorsPackagingIssue>& Issues,
		EOpenMobileSensorsPackagingIssueCode Code
	)
	{
		return Issues.ContainsByPredicate(
			[Code](const FOpenMobileSensorsPackagingIssue& Issue)
			{
				return Issue.Code == Code;
			}
		);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPackagingValidationTest,
	"OpenMobile.Sensors.Packaging.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPackagingValidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPackagingTestsPrivate;
	TArray<FOpenMobileSensorsPackagingIssue> Issues;

	UOpenMobileSensorsSettings* Settings =
		NewObject<UOpenMobileSensorsSettings>();
	FOpenMobileSensorsPackagingContext Android;
	Android.Target = EOpenMobileSensorsPackagingTarget::Android;
	Android.bSensorsPlatformPackagingEnabled = true;
	TestTrue(TEXT("Default Android packaging is valid"),
		FOpenMobileSensorsPackagingValidator::Validate(
			*Settings,
			Android,
			Issues
		));

	Settings->bEnablePermissionSensitiveSensors = true;
	Android.bSensorsPlatformPackagingEnabled = false;
	TestFalse(TEXT("Activity features require a declaration owner"),
		FOpenMobileSensorsPackagingValidator::Validate(
			*Settings,
			Android,
			Issues
		));
	TestTrue(TEXT("Missing activity declaration is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::MissingAndroidActivityDeclaration
		));
	Android.bActivityProviderPackagingEnabled = true;
	TestTrue(TEXT("An enabled activity provider may own its declaration"),
		FOpenMobileSensorsPackagingValidator::Validate(
			*Settings,
			Android,
			Issues
		));

	Settings->bEnablePermissionSensitiveSensors = false;
	Settings->bAllowHighSamplingRate = true;
	Android.bActivityProviderPackagingEnabled = false;
	TestFalse(TEXT("High sampling requires the Sensors Android path"),
		FOpenMobileSensorsPackagingValidator::Validate(
			*Settings,
			Android,
			Issues
		));
	TestTrue(TEXT("Missing high-sampling declaration is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::MissingAndroidHighSamplingDeclaration
		));

	Settings->bAllowHighSamplingRate = false;
	Android.bSensorsPlatformPackagingEnabled = true;
	Android.AndroidHostDeclarations = {
		TEXT("android.permission.HIGH_SAMPLING_RATE_SENSORS")
	};
	TestFalse(TEXT("Host permissions cannot contradict canonical settings"),
		FOpenMobileSensorsPackagingValidator::Validate(
			*Settings,
			Android,
			Issues
		));
	TestTrue(TEXT("Contradictory host declaration is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::MismatchedAndroidHostDeclaration
		));

	FOpenMobileSensorsPackagingContext IOS;
	IOS.Target = EOpenMobileSensorsPackagingTarget::IOS;
	IOS.bSensorsPlatformPackagingEnabled = false;
	TestFalse(TEXT("iOS motion requires the Sensors iOS packaging path"),
		FOpenMobileSensorsPackagingValidator::Validate(*Settings, IOS, Issues));
	TestTrue(TEXT("Missing iOS packaging path is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::MissingIOSMotionDeclaration
		));

	IOS.bSensorsPlatformPackagingEnabled = true;
	Settings->IOSMotionUsageDescription.Reset();
	TestFalse(TEXT("iOS motion requires nonempty purpose text"),
		FOpenMobileSensorsPackagingValidator::Validate(*Settings, IOS, Issues));
	TestTrue(TEXT("Missing purpose text is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::MissingIOSMotionUsageDescription
		));

	Settings->IOSMotionUsageDescription = TEXT("Uses motion for steering.");
	IOS.bIOSHostPlistFallbackRequired = true;
	IOS.IOSHostAdditionalPlistData.Reset();
	TestFalse(TEXT("UE 5.8 requires a host plist fallback"),
		FOpenMobileSensorsPackagingValidator::Validate(*Settings, IOS, Issues));
	TestTrue(TEXT("Missing UE 5.8 fallback is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::MissingIOSMotionHostFallback
		));

	IOS.IOSHostAdditionalPlistData =
		TEXT("<key>NSMotionUsageDescription</key><string>Uses motion for steering.</string>");
	TestTrue(TEXT("A matching UE 5.8 host fallback is valid"),
		FOpenMobileSensorsPackagingValidator::Validate(*Settings, IOS, Issues));
	IOS.IOSHostAdditionalPlistData =
		TEXT("<key>NSMotionUsageDescription</key><string>Uses motion for steering.</string>")
		TEXT("<key>NSMotionUsageDescription</key><string>Uses motion for steering.</string>");
	TestFalse(TEXT("UE 5.8 rejects repeated host fallback keys"),
		FOpenMobileSensorsPackagingValidator::Validate(*Settings, IOS, Issues));
	TestTrue(TEXT("Repeated host fallback is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::DuplicateIOSMotionUsageDescription
		));

	IOS.IOSHostAdditionalPlistData =
		TEXT("<key>NSMotionUsageDescription</key><string>Other text.</string>");
	TestFalse(TEXT("A mismatched host purpose string is rejected"),
		FOpenMobileSensorsPackagingValidator::Validate(*Settings, IOS, Issues));
	TestTrue(TEXT("Mismatched host purpose text is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::MismatchedIOSMotionUsageDescription
		));

	IOS.bIOSHostPlistFallbackRequired = false;
	IOS.IOSHostAdditionalPlistData =
		TEXT("<key>NSMotionUsageDescription</key><string>Uses motion for steering.</string>");
	TestFalse(TEXT("Fixed build paths reject a host duplicate"),
		FOpenMobileSensorsPackagingValidator::Validate(*Settings, IOS, Issues));
	TestTrue(TEXT("Host plist duplicate is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::DuplicateIOSMotionUsageDescription
		));

	IOS.IOSHostAdditionalPlistData.Reset();
	IOS.bShipping = true;
	Settings->DevelopmentInputMode =
		EOpenMobileSensorsDevelopmentInputMode::Mock;
	TestFalse(TEXT("Shipping packaging rejects mock input"),
		FOpenMobileSensorsPackagingValidator::Validate(*Settings, IOS, Issues));
	TestTrue(TEXT("Unsafe Shipping input is explicit"),
		HasIssue(
			Issues,
			EOpenMobileSensorsPackagingIssueCode::UnsafeShippingDevelopmentInput
		));
	return true;
}

#endif

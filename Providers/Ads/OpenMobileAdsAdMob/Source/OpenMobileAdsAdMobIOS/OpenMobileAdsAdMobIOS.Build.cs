using System.IO;
using UnrealBuildTool;

public class OpenMobileAdsAdMobIOS : ModuleRules
{
	/** Links Google Mobile Ads, UMP, privacy resources, and iOS receipt data only in the native backend. */
	public OpenMobileAdsAdMobIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bEnableObjCAutomaticReferenceCounting = true;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"Core",
			"OpenMobileAdsAdMob",
			"Swift"
		});

		PublicFrameworks.AddRange(new[]
		{
			"AdSupport",
			"AppTrackingTransparency",
			"AudioToolbox",
			"AVFoundation",
			"CoreGraphics",
			"CoreMedia",
			"CoreTelephony",
			"JavaScriptCore",
			"MessageUI",
			"SafariServices",
			"StoreKit",
			"SystemConfiguration",
			"UIKit",
			"WebKit"
		});

		string ThirdPartyIOSPath = Path.Combine(ModuleDirectory, "../../ThirdParty/IOS");
		PublicAdditionalFrameworks.Add(new Framework(
			"GoogleMobileAds",
			Path.Combine(ThirdPartyIOSPath, "GoogleMobileAds.xcframework.zip"),
			Framework.FrameworkMode.LinkAndCopy
		));
		PublicAdditionalFrameworks.Add(new Framework(
			"UserMessagingPlatform",
			Path.Combine(ThirdPartyIOSPath, "UserMessagingPlatform.xcframework.zip"),
			Framework.FrameworkMode.LinkAndCopy
		));

		string PrivacyBundlePath = Path.Combine(
			ModuleDirectory,
			"../../Resources/IOS/OpenMobileAdsAdMobPrivacy.bundle"
		);
		AdditionalBundleResources.Add(new BundleResource(PrivacyBundlePath, ""));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "Info.plist"));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "PrivacyInfo.xcprivacy"));
		ExternalDependencies.Add(Path.Combine(ModuleDirectory, "../../apple-metadata.json"));

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/IOS/OpenMobileAdsAdMob_IOS_UPL.xml")
		);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileAdsAdMob_IOS_UPL.xml")
		);
	}
}

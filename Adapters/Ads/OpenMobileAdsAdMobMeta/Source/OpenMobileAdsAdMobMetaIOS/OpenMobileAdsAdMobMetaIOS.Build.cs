using System.IO;
using UnrealBuildTool;

public class OpenMobileAdsAdMobMetaIOS : ModuleRules
{
	public OpenMobileAdsAdMobMetaIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileAdsAdMob",
			"Swift"
		});

		PublicFrameworks.AddRange(new[]
		{
			"AppTrackingTransparency",
			"AudioToolbox",
			"AVFoundation",
			"CoreGraphics",
			"CoreMedia",
			"CoreTelephony",
			"StoreKit",
			"UIKit",
			"WebKit"
		});

		string ThirdPartyIOSPath = Path.Combine(ModuleDirectory, "../../ThirdParty/IOS");
		PublicAdditionalFrameworks.Add(new Framework(
			"MetaAdapter",
			Path.Combine(ThirdPartyIOSPath, "MetaAdapter.xcframework.zip"),
			Framework.FrameworkMode.Link
		));
		PublicAdditionalFrameworks.Add(new Framework(
			"FBAudienceNetwork",
			Path.Combine(ThirdPartyIOSPath, "FBAudienceNetwork.xcframework.zip"),
			Framework.FrameworkMode.LinkAndCopy
		));

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/IOS/OpenMobileAdsAdMobMeta_IOS_UPL.xml")
		);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileAdsAdMobMeta_IOS_UPL.xml")
		);
	}
}

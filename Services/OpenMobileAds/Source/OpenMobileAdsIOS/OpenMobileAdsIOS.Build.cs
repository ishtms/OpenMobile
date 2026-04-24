using System.IO;
using UnrealBuildTool;

public class OpenMobileAdsIOS : ModuleRules
{
	public OpenMobileAdsIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bEnableObjCAutomaticReferenceCounting = true;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileAds"
		});

		PublicFrameworks.AddRange(new[]
		{
			"AppTrackingTransparency",
			"UIKit"
		});

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		string IOSPluginPath = Path.Combine(
			ModuleDirectory,
			"Private/IOS/OpenMobileAds_IOS_UPL.xml"
		);
		ExternalDependencies.Add(IOSPluginPath);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileAds_IOS_UPL.xml")
		);
	}
}

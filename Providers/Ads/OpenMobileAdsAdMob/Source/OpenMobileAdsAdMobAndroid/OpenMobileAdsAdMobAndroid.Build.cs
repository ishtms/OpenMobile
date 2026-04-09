using System.IO;
using UnrealBuildTool;

public class OpenMobileAdsAdMobAndroid : ModuleRules
{
	public OpenMobileAdsAdMobAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"Launch",
			"OpenMobileAdsAdMob"
		});
		PrivateIncludePathModuleNames.Add("Launch");

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		AdditionalPropertiesForReceipt.Add(
			"AndroidPlugin",
			Path.Combine(ModulePath, "Private/Android/OpenMobileAdsAdMob_Android_UPL.xml")
		);
	}
}

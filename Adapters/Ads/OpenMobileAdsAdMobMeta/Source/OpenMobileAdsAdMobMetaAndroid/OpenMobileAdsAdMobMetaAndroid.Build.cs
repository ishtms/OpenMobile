using System.IO;
using UnrealBuildTool;

public class OpenMobileAdsAdMobMetaAndroid : ModuleRules
{
	public OpenMobileAdsAdMobMetaAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileAdsAdMob"
		});

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/Android/OpenMobileAdsAdMobMeta_Android_UPL.xml")
		);
		AdditionalPropertiesForReceipt.Add(
			"AndroidPlugin",
			Path.Combine(ModulePath, "Private/Android/OpenMobileAdsAdMobMeta_Android_UPL.xml")
		);
	}
}

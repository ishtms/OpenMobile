using System.IO;
using UnrealBuildTool;

public class OpenMobileAdsAdMobChartboostAndroid : ModuleRules
{
	public OpenMobileAdsAdMobChartboostAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"Launch",
			"OpenMobileAds",
			"OpenMobileAdsAdMob"
		});
		PrivateIncludePathModuleNames.Add("Launch");

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/Android/OpenMobileAdsAdMobChartboost_Android_UPL.xml")
		);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/Android/OpenMobileAdsAdMobChartboost_Dependencies.gradle")
		);
		AdditionalPropertiesForReceipt.Add(
			"AndroidPlugin",
			Path.Combine(ModulePath, "Private/Android/OpenMobileAdsAdMobChartboost_Android_UPL.xml")
		);
	}
}

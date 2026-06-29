using System.IO;
using UnrealBuildTool;

public class OpenMobileAdsAdMobUnityAndroid : ModuleRules
{
	public OpenMobileAdsAdMobUnityAndroid(ReadOnlyTargetRules Target) : base(Target)
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
			Path.Combine(ModuleDirectory, "Private/Android/OpenMobileAdsAdMobUnity_Android_UPL.xml")
		);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/Android/OpenMobileAdsAdMobUnity_Dependencies.gradle")
		);
		AdditionalPropertiesForReceipt.Add(
			"AndroidPlugin",
			Path.Combine(ModulePath, "Private/Android/OpenMobileAdsAdMobUnity_Android_UPL.xml")
		);
	}
}

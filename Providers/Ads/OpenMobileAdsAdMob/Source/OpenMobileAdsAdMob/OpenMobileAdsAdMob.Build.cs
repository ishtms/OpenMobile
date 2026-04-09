using UnrealBuildTool;

public class OpenMobileAdsAdMob : ModuleRules
{
	public OpenMobileAdsAdMob(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"OpenMobileAds",
			"OpenMobileCore"
		});
	}
}

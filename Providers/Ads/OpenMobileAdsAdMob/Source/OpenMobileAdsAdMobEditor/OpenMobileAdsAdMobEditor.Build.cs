using UnrealBuildTool;

public class OpenMobileAdsAdMobEditor : ModuleRules
{
	/** Keeps AdMob identifier validation in editor targets and reuses the shared Ads Message Log. */
	public OpenMobileAdsAdMobEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"MessageLog",
			"OpenMobileAds",
			"OpenMobileAdsEditor",
			"OpenMobileAdsAdMob",
			"PropertyEditor",
			"Projects",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}

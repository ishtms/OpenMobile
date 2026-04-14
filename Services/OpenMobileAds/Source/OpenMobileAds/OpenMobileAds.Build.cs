using EpicGames.Core;
using UnrealBuildTool;

public class OpenMobileAds : ModuleRules
{
	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAds.OpenMobileAdsSettings")]
	bool bDevelopmentTestMode = false;

	public OpenMobileAds(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"OpenMobileCore"
		});

		if (Target.Configuration == UnrealTargetConfiguration.Shipping && Target.ProjectFile != null)
		{
			ConfigCache.ReadSettings(
				DirectoryReference.FromFile(Target.ProjectFile),
				Target.Platform,
				this
			);
			if (bDevelopmentTestMode)
			{
				throw new BuildException(
					"OpenMobile Ads Development/Test Mode must be disabled for Shipping builds."
				);
			}
		}
	}
}

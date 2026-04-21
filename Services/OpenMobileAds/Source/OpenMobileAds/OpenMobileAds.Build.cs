using System.Collections.Generic;
using EpicGames.Core;
using UnrealBuildTool;

public class OpenMobileAds : ModuleRules
{
	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAds.OpenMobileAdsSettings")]
	bool bDevelopmentTestMode = false;

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAds.OpenMobileAdsSettings")]
	List<string> TestDeviceIdentifiers = new List<string>();

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

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Slate",
			"SlateCore"
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
			if (TestDeviceIdentifiers.Count > 0)
			{
				throw new BuildException(
					"OpenMobile Ads global test-device identifiers must be removed before making a Shipping build."
				);
			}
		}
	}
}

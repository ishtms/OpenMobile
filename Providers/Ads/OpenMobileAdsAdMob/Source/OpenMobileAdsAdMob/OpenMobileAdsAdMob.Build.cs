using System;
using System.Collections.Generic;
using EpicGames.Core;
using UnrealBuildTool;

public class OpenMobileAdsAdMob : ModuleRules
{
	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAdsAdMob.OpenMobileAdsAdMobSettings")]
	string AndroidAppId = "ca-app-pub-3940256099942544~3347511713";

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAdsAdMob.OpenMobileAdsAdMobSettings")]
	string AndroidRewardedAdUnitId = "ca-app-pub-3940256099942544/5224354917";

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAdsAdMob.OpenMobileAdsAdMobSettings")]
	string IOSAppId = "ca-app-pub-3940256099942544~1458002511";

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAdsAdMob.OpenMobileAdsAdMobSettings")]
	string IOSRewardedAdUnitId = "ca-app-pub-3940256099942544/1712485313";

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAdsAdMob.OpenMobileAdsAdMobSettings")]
	List<string> TestDeviceIdentifiers = new List<string>();

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

		if (Target.Configuration == UnrealTargetConfiguration.Shipping && Target.ProjectFile != null)
		{
			ConfigCache.ReadSettings(
				DirectoryReference.FromFile(Target.ProjectFile),
				Target.Platform,
				this
			);
			if (TestDeviceIdentifiers.Count > 0)
			{
				throw new BuildException(
					"OpenMobile Ads AdMob test-device identifiers must be removed before making a Shipping build."
				);
			}
			string[] Identifiers =
			{
				AndroidAppId,
				AndroidRewardedAdUnitId,
				IOSAppId,
				IOSRewardedAdUnitId
			};
			foreach (string Identifier in Identifiers)
			{
				if (Identifier.Trim().StartsWith(
					"ca-app-pub-3940256099942544",
					StringComparison.Ordinal
				))
				{
					throw new BuildException(
						"OpenMobile Ads AdMob sample IDs must be replaced before making a Shipping build."
					);
				}
			}
		}
	}
}

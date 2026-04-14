using System;
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
			string[] Identifiers =
			{
				AndroidAppId,
				AndroidRewardedAdUnitId,
				IOSAppId,
				IOSRewardedAdUnitId
			};
			string[] GoogleSampleIdentifiers =
			{
				"ca-app-pub-3940256099942544~3347511713",
				"ca-app-pub-3940256099942544/5224354917",
				"ca-app-pub-3940256099942544~1458002511",
				"ca-app-pub-3940256099942544/1712485313"
			};
			foreach (string Identifier in Identifiers)
			{
				if (Array.IndexOf(GoogleSampleIdentifiers, Identifier) >= 0)
				{
					throw new BuildException(
						"OpenMobile Ads AdMob sample IDs must be replaced before making a Shipping build."
					);
				}
			}
		}
	}
}

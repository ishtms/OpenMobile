using System;
using System.Collections.Generic;
using EpicGames.Core;
using UnrealBuildTool;

public class OpenMobileAdsAdMob : ModuleRules
{
	private static void ValidateAndroidAppId(string Value)
	{
		string TrimmedValue = Value.Trim();
		bool ContainsWhitespace = false;
		foreach (char Character in Value)
		{
			if (char.IsWhiteSpace(Character))
			{
				ContainsWhitespace = true;
				break;
			}
		}
		if (
			TrimmedValue.Length == 0
			|| ContainsWhitespace
			|| !TrimmedValue.StartsWith("ca-app-pub-", StringComparison.Ordinal)
			|| !TrimmedValue.Contains("~", StringComparison.Ordinal)
		)
		{
			throw new BuildException(
				"OpenMobile Ads AdMob Android app ID must start with 'ca-app-pub-', contain '~', and have no whitespace."
			);
		}
	}

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

		if (Target.ProjectFile == null)
		{
			return;
		}

		if (
			Target.Platform == UnrealTargetPlatform.Android
			|| Target.Configuration == UnrealTargetConfiguration.Shipping
		)
		{
			ConfigCache.ReadSettings(
				DirectoryReference.FromFile(Target.ProjectFile),
				Target.Platform,
				this
			);
		}
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			ValidateAndroidAppId(AndroidAppId);
		}

		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
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

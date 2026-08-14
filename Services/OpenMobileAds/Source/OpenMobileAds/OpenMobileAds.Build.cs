using System;
using System.Collections.Generic;
using EpicGames.Core;
using UnrealBuildTool;

public class OpenMobileAds : ModuleRules
{
	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAds.OpenMobileAdsSettings")]
	bool bDevelopmentTestMode = false;

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAds.OpenMobileAdsSettings")]
	string DebugGeography = "Disabled";

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAds.OpenMobileAdsSettings")]
	List<string> TestDeviceIdentifiers = new List<string>();

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAds.OpenMobileAdsSettings")]
	bool bEnableTrackingAuthorization = false;

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenMobileAds.OpenMobileAdsSettings")]
	string TrackingUsageDescription = "";

	/** Keeps provider SDKs out of the service module while enforcing shipping and iOS privacy settings at build time. */
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

		bool bShipping = Target.Configuration == UnrealTargetConfiguration.Shipping;
		bool bIOS = Target.Platform == UnrealTargetPlatform.IOS;
		if (Target.ProjectFile != null && (bShipping || bIOS))
		{
			ConfigCache.ReadSettings(
				DirectoryReference.FromFile(Target.ProjectFile),
				Target.Platform,
				this
			);
		}
		if (bShipping)
		{
			if (bDevelopmentTestMode)
			{
				throw new BuildException(
					"OpenMobile Ads Development/Test Mode must be disabled for Shipping builds."
				);
			}
			if (!string.Equals(DebugGeography, "Disabled", StringComparison.OrdinalIgnoreCase))
			{
				throw new BuildException(
					"OpenMobile Ads Consent Debug Geography must be disabled for Shipping builds."
				);
			}
			if (TestDeviceIdentifiers.Count > 0)
			{
				throw new BuildException(
					"OpenMobile Ads global test-device identifiers must be removed before making a Shipping build."
				);
			}
		}
		if (
			bIOS
			&& bEnableTrackingAuthorization
			&& !ValidateTrackingUsageDescription(TrackingUsageDescription)
		)
		{
			throw new BuildException(
				"OpenMobile Ads App Tracking Transparency requires a non-empty Tracking Usage Description of at most 1024 characters without control characters."
			);
		}
	}

	/** Rejects empty, oversized, or control-character prompt text before iOS packaging. */
	static bool ValidateTrackingUsageDescription(string Description)
	{
		if (string.IsNullOrWhiteSpace(Description))
		{
			return false;
		}
		string Trimmed = Description.Trim();
		if (Trimmed.Length > 1024)
		{
			return false;
		}
		foreach (char Character in Trimmed)
		{
			if (Character < 0x20 || Character == 0x7f)
			{
				return false;
			}
		}
		return true;
	}
}

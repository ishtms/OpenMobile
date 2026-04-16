using System;
using System.Collections.Generic;
using System.IO;
using System.Xml.Linq;
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

	private static void ValidateIOSAppId(string Value)
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
				"OpenMobile Ads AdMob iOS app ID must start with 'ca-app-pub-', contain '~', and have no whitespace."
			);
		}
	}

	private static HashSet<string> ReadRequiredSKAdNetworkIdentifiers(string UPLPath)
	{
		HashSet<string> Identifiers = new HashSet<string>(StringComparer.Ordinal);
		try
		{
			XDocument Document = XDocument.Load(UPLPath);
			foreach (XElement Element in Document.Descendants())
			{
				string Value = Element.Value.Trim();
				if (Element.Name.LocalName == "string" && Value.EndsWith(".skadnetwork", StringComparison.Ordinal))
				{
					Identifiers.Add(Value);
				}
			}
		}
		catch (Exception Exception)
		{
			throw new BuildException(
				Exception,
				"OpenMobile Ads could not read the AdMob iOS plist contract."
			);
		}
		if (Identifiers.Count == 0)
		{
			throw new BuildException(
				"OpenMobile Ads found no SKAdNetwork identifiers in the AdMob iOS plist contract."
			);
		}
		return Identifiers;
	}

	private static void ValidateAdditionalPlistData(
		string Value,
		string ExpectedAppId,
		bool bRequireCompleteProviderValues,
		string UPLPath
	)
	{
		if (String.IsNullOrWhiteSpace(Value))
		{
			if (bRequireCompleteProviderValues)
			{
				throw new BuildException(
					"OpenMobile Ads AdMob requires its app ID and SKAdNetworkItems in iOS Additional Plist Data for a direct Xcode build."
				);
			}
			return;
		}

		XElement Root;
		try
		{
			Root = XElement.Parse("<dict>" + Value + "</dict>");
		}
		catch (Exception Exception)
		{
			throw new BuildException(
				Exception,
				"OpenMobile Ads could not parse iOS Additional Plist Data."
			);
		}

		List<XElement> Elements = new List<XElement>(Root.Elements());
		Dictionary<string, XElement> Entries = new Dictionary<string, XElement>(
			StringComparer.Ordinal
		);
		for (int Index = 0; Index < Elements.Count; Index += 2)
		{
			if (Elements[Index].Name.LocalName != "key" || Index + 1 >= Elements.Count)
			{
				throw new BuildException(
					"OpenMobile Ads iOS Additional Plist Data must contain key and value pairs."
				);
			}
			string Key = Elements[Index].Value;
			if (Entries.ContainsKey(Key))
			{
				throw new BuildException(
					$"OpenMobile Ads found duplicate iOS plist key '{Key}' in Additional Plist Data."
				);
			}
			Entries.Add(Key, Elements[Index + 1]);
		}

		if (Entries.TryGetValue("GADApplicationIdentifier", out XElement AppIdElement))
		{
			if (
				AppIdElement.Name.LocalName != "string"
				|| !String.Equals(AppIdElement.Value.Trim(), ExpectedAppId.Trim(), StringComparison.Ordinal)
			)
			{
				throw new BuildException(
					"OpenMobile Ads iOS Additional Plist Data has a GADApplicationIdentifier that conflicts with the AdMob iOS app ID."
				);
			}
		}

		HashSet<string> Identifiers = new HashSet<string>(StringComparer.Ordinal);
		if (Entries.TryGetValue("SKAdNetworkItems", out XElement SKAdItemsElement))
		{
			if (SKAdItemsElement.Name.LocalName != "array")
			{
				throw new BuildException(
					"OpenMobile Ads iOS SKAdNetworkItems in Additional Plist Data must be an array."
				);
			}
			foreach (XElement Item in SKAdItemsElement.Elements())
			{
				List<XElement> ItemElements = new List<XElement>(Item.Elements());
				if (
					Item.Name.LocalName != "dict"
					|| ItemElements.Count != 2
					|| ItemElements[0].Name.LocalName != "key"
					|| ItemElements[0].Value != "SKAdNetworkIdentifier"
					|| ItemElements[1].Name.LocalName != "string"
					|| String.IsNullOrWhiteSpace(ItemElements[1].Value)
				)
				{
					throw new BuildException(
						"OpenMobile Ads iOS SKAdNetworkItems entries must contain one string SKAdNetworkIdentifier."
					);
				}
				if (!Identifiers.Add(ItemElements[1].Value.Trim()))
				{
					throw new BuildException(
						$"OpenMobile Ads found duplicate SKAdNetworkIdentifier '{ItemElements[1].Value.Trim()}' in Additional Plist Data."
					);
				}
			}
		}

		if (
			Entries.TryGetValue("NSUserTrackingUsageDescription", out XElement TrackingElement)
			&& (
				TrackingElement.Name.LocalName != "string"
				|| String.IsNullOrWhiteSpace(TrackingElement.Value)
			)
		)
		{
			throw new BuildException(
				"OpenMobile Ads iOS tracking usage description must be a non-empty string when configured."
			);
		}

		if (bRequireCompleteProviderValues)
		{
			if (!Entries.ContainsKey("GADApplicationIdentifier"))
			{
				throw new BuildException(
					"OpenMobile Ads AdMob requires GADApplicationIdentifier in iOS Additional Plist Data for a direct Xcode build."
				);
			}
			if (!Entries.ContainsKey("SKAdNetworkItems"))
			{
				throw new BuildException(
					"OpenMobile Ads AdMob requires SKAdNetworkItems in iOS Additional Plist Data for a direct Xcode build."
				);
			}
			HashSet<string> RequiredIdentifiers = ReadRequiredSKAdNetworkIdentifiers(UPLPath);
			foreach (string Identifier in RequiredIdentifiers)
			{
				if (!Identifiers.Contains(Identifier))
				{
					throw new BuildException(
						$"OpenMobile Ads AdMob direct Xcode build is missing SKAdNetworkIdentifier '{Identifier}' in iOS Additional Plist Data."
					);
				}
			}
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

	[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings")]
	string AdditionalPlistData = "";

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
			|| Target.Platform == UnrealTargetPlatform.IOS
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
		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			ValidateIOSAppId(IOSAppId);
			ValidateAdditionalPlistData(
				AdditionalPlistData,
				IOSAppId,
				Environment.GetEnvironmentVariable("UE_BUILD_FROM_XCODE") == "1",
				Path.Combine(
					ModuleDirectory,
					"../OpenMobileAdsAdMobIOS/Private/IOS/OpenMobileAdsAdMob_IOS_UPL.xml"
				)
			);
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

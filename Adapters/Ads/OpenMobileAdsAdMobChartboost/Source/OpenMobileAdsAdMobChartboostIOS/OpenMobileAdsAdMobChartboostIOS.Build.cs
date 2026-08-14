using System;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

public class OpenMobileAdsAdMobChartboostIOS : ModuleRules
{
	/** Reads the checked-in Chartboost, adapter, or Google package version by name. */
	private static string ReadPackageVersion(string ManifestPath, string PackageName)
	{
		JsonObject Document = JsonObject.Read(new FileReference(ManifestPath));
		foreach (JsonObject Package in Document.GetObjectArrayField("packages"))
		{
			if (Package.GetStringField("name") == PackageName)
			{
				return Package.GetStringField("version");
			}
		}
		throw new BuildException(
			$"OpenMobile Ads AdMob Chartboost could not find package '{PackageName}' in '{ManifestPath}'."
		);
	}

	/** Stops unsupported Chartboost versions and flags supported combinations that haven't been tested. */
	private static void ValidateVersion(
		string Name,
		string Actual,
		JsonObject Compatibility,
		ILogger Logger
	)
	{
		string MinimumText = Compatibility.GetStringField("minimum");
		string MaximumText = Compatibility.GetStringField("maximum_exclusive");
		if (
			!Version.TryParse(Actual, out Version ActualVersion)
			|| !Version.TryParse(MinimumText, out Version MinimumVersion)
			|| !Version.TryParse(MaximumText, out Version MaximumVersion)
		)
		{
			Logger.LogWarning(
				"OpenMobile Ads AdMob Chartboost could not parse the {Name} compatibility values.",
				Name
			);
			return;
		}
		if (ActualVersion < MinimumVersion || ActualVersion >= MaximumVersion)
		{
			throw new BuildException(
				$"OpenMobile Ads AdMob Chartboost {Name} {Actual} is outside supported range " +
				$"[{MinimumText}, {MaximumText})."
			);
		}

		bool bTested = false;
		foreach (string Tested in Compatibility.GetStringArrayField("tested"))
		{
			if (Tested == Actual)
			{
				bTested = true;
				break;
			}
		}
		if (!bTested)
		{
			Logger.LogWarning(
				"OpenMobile Ads AdMob Chartboost {Name} {Actual} is supported but not tested.",
				Name,
				Actual
			);
		}
	}

	/** Verifies the three pinned iOS package versions against adapter compatibility metadata. */
	private static string[] ValidateCompatibility(string ModulePath, ILogger Logger)
	{
		string AdapterRoot = Path.GetFullPath(Path.Combine(ModulePath, "../.."));
		string ProviderRoot = Path.GetFullPath(Path.Combine(
			ModulePath,
			"../../../../../Providers/Ads/OpenMobileAdsAdMob"
		));
		string AdapterManifestPath = Path.Combine(AdapterRoot, "adapter.json");
		string AdapterPackagesPath = Path.Combine(AdapterRoot, "ThirdParty/IOS/packages.json");
		string ProviderPackagesPath = Path.Combine(ProviderRoot, "ThirdParty/IOS/packages.json");
		JsonObject IOS = JsonObject.Read(new FileReference(AdapterManifestPath))
			.GetObjectField("platforms")
			.GetObjectField("IOS");
		JsonObject Compatibility = IOS.GetObjectField("compatibility");

		string ProviderVersion = ReadPackageVersion(
			ProviderPackagesPath,
			"Google Mobile Ads SDK for iOS"
		);
		string AdapterVersion = ReadPackageVersion(
			AdapterPackagesPath,
			"Google Mobile Ads Mediation Adapter for Chartboost"
		);
		string NetworkVersion = ReadPackageVersion(
			AdapterPackagesPath,
			"Chartboost SDK for iOS"
		);
		if (AdapterVersion != IOS.GetStringField("adapter_version"))
		{
			throw new BuildException("OpenMobile Ads AdMob Chartboost adapter metadata conflicts with its iOS package manifest.");
		}
		if (NetworkVersion != IOS.GetStringField("network_sdk_version"))
		{
			throw new BuildException("OpenMobile Ads AdMob Chartboost network metadata conflicts with its iOS package manifest.");
		}

		ValidateVersion("provider SDK", ProviderVersion, Compatibility.GetObjectField("provider_sdk"), Logger);
		ValidateVersion("adapter", AdapterVersion, Compatibility.GetObjectField("adapter"), Logger);
		ValidateVersion("network SDK", NetworkVersion, Compatibility.GetObjectField("network_sdk"), Logger);
		return new[] { AdapterManifestPath, AdapterPackagesPath, ProviderPackagesPath };
	}

	/** Links Chartboost adapter payloads and privacy bundles only in opted-in iOS builds. */
	public OpenMobileAdsAdMobChartboostIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bEnableObjCAutomaticReferenceCounting = true;
		ExternalDependencies.AddRange(ValidateCompatibility(ModuleDirectory, Target.Logger));

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileAds",
			"OpenMobileAdsAdMob",
			"Swift"
		});

		PublicFrameworks.AddRange(new[]
		{
			"AVFoundation",
			"CoreGraphics",
			"Foundation",
			"StoreKit",
			"UIKit",
			"WebKit"
		});

		string ThirdPartyIOSPath = Path.Combine(ModuleDirectory, "../../ThirdParty/IOS");
		PublicAdditionalFrameworks.Add(new Framework(
			"ChartboostAdapter",
			Path.Combine(ThirdPartyIOSPath, "ChartboostAdapter.xcframework.zip"),
			Framework.FrameworkMode.Link
		));
		PublicAdditionalFrameworks.Add(new Framework(
			"ChartboostSDK",
			Path.Combine(ThirdPartyIOSPath, "ChartboostSDK.xcframework.zip"),
			Framework.FrameworkMode.Link
		));

		string PrivacyBundlePath = Path.Combine(
			ModuleDirectory,
			"../../Resources/IOS/OpenMobileAdsAdMobChartboostPrivacy.bundle"
		);
		AdditionalBundleResources.Add(new BundleResource(PrivacyBundlePath, ""));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "Info.plist"));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "PrivacyInfo.xcprivacy"));

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/IOS/OpenMobileAdsAdMobChartboost_IOS_UPL.xml")
		);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileAdsAdMobChartboost_IOS_UPL.xml")
		);
	}
}

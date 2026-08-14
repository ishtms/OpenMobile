using System;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

public class OpenMobileAdsAdMobLiftoffMonetizeIOS : ModuleRules
{
	/** Finds one Liftoff-related package version in the checked-in iOS manifests. */
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
			$"OpenMobile Ads AdMob Liftoff Monetize could not find package '{PackageName}' in '{ManifestPath}'."
		);
	}

	/** Enforces Liftoff compatibility limits and warns when an allowed version lacks test evidence. */
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
				"OpenMobile Ads AdMob Liftoff Monetize could not parse the {Name} compatibility values.",
				Name
			);
			return;
		}
		if (ActualVersion < MinimumVersion || ActualVersion >= MaximumVersion)
		{
			throw new BuildException(
				$"OpenMobile Ads AdMob Liftoff Monetize {Name} {Actual} is outside supported range " +
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
				"OpenMobile Ads AdMob Liftoff Monetize {Name} {Actual} is supported but not tested.",
				Name,
				Actual
			);
		}
	}

	/** Checks Google, Liftoff adapter, and Vungle SDK versions before native linking starts. */
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
			"Google Mobile Ads Mediation Adapter for Liftoff Monetize"
		);
		string NetworkVersion = ReadPackageVersion(
			AdapterPackagesPath,
			"Vungle Ads SDK for iOS"
		);
		if (AdapterVersion != IOS.GetStringField("adapter_version"))
		{
			throw new BuildException("OpenMobile Ads AdMob Liftoff Monetize adapter metadata conflicts with its iOS package manifest.");
		}
		if (NetworkVersion != IOS.GetStringField("network_sdk_version"))
		{
			throw new BuildException("OpenMobile Ads AdMob Liftoff Monetize network metadata conflicts with its iOS package manifest.");
		}

		ValidateVersion("provider SDK", ProviderVersion, Compatibility.GetObjectField("provider_sdk"), Logger);
		ValidateVersion("adapter", AdapterVersion, Compatibility.GetObjectField("adapter"), Logger);
		ValidateVersion("network SDK", NetworkVersion, Compatibility.GetObjectField("network_sdk"), Logger);
		return new[] { AdapterManifestPath, AdapterPackagesPath, ProviderPackagesPath };
	}

	/** Links Liftoff Monetize frameworks and privacy resources only for enabled iOS targets. */
	public OpenMobileAdsAdMobLiftoffMonetizeIOS(ReadOnlyTargetRules Target) : base(Target)
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
			"AdSupport",
			"AudioToolbox",
			"AVFoundation",
			"CFNetwork",
			"CoreGraphics",
			"CoreMedia",
			"Foundation",
			"MediaPlayer",
			"QuartzCore",
			"StoreKit",
			"SystemConfiguration",
			"UIKit",
			"WebKit"
		});
		PublicSystemLibraries.Add("z");

		string ThirdPartyIOSPath = Path.Combine(ModuleDirectory, "../../ThirdParty/IOS");
		PublicAdditionalFrameworks.Add(new Framework(
			"LiftoffMonetizeAdapter",
			Path.Combine(ThirdPartyIOSPath, "LiftoffMonetizeAdapter.xcframework.zip"),
			Framework.FrameworkMode.Link
		));
		PublicAdditionalFrameworks.Add(new Framework(
			"VungleAdsSDK",
			Path.Combine(ThirdPartyIOSPath, "VungleAdsSDK.xcframework.zip"),
			Framework.FrameworkMode.Link
		));

		string PrivacyBundlePath = Path.Combine(
			ModuleDirectory,
			"../../Resources/IOS/OpenMobileAdsAdMobLiftoffMonetizePrivacy.bundle"
		);
		AdditionalBundleResources.Add(new BundleResource(PrivacyBundlePath, ""));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "Info.plist"));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "PrivacyInfo.xcprivacy"));

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/IOS/OpenMobileAdsAdMobLiftoffMonetize_IOS_UPL.xml")
		);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileAdsAdMobLiftoffMonetize_IOS_UPL.xml")
		);
	}
}

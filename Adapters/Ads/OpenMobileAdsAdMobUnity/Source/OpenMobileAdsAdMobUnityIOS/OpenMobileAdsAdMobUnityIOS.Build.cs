using System;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

public class OpenMobileAdsAdMobUnityIOS : ModuleRules
{
	/** Looks up Unity, adapter, and Google versions from the checked-in package manifests. */
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
			$"OpenMobile Ads AdMob Unity could not find package '{PackageName}' in '{ManifestPath}'."
		);
	}

	/** Enforces Unity compatibility bounds and warns when an allowed version is still untested. */
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
				"OpenMobile Ads AdMob Unity could not parse the {Name} compatibility values.",
				Name
			);
			return;
		}
		if (ActualVersion < MinimumVersion || ActualVersion >= MaximumVersion)
		{
			throw new BuildException(
				$"OpenMobile Ads AdMob Unity {Name} {Actual} is outside supported range " +
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
				"OpenMobile Ads AdMob Unity {Name} {Actual} is supported but not tested.",
				Name,
				Actual
			);
		}
	}

	/** Validates provider, Unity adapter, and Unity Ads SDK versions before linking. */
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
			"Google Mobile Ads Mediation Adapter for Unity"
		);
		string NetworkVersion = ReadPackageVersion(
			AdapterPackagesPath,
			"Unity Ads SDK for iOS"
		);
		if (AdapterVersion != IOS.GetStringField("adapter_version"))
		{
			throw new BuildException("OpenMobile Ads AdMob Unity adapter metadata conflicts with its iOS package manifest.");
		}
		if (NetworkVersion != IOS.GetStringField("network_sdk_version"))
		{
			throw new BuildException("OpenMobile Ads AdMob Unity network metadata conflicts with its iOS package manifest.");
		}

		ValidateVersion("provider SDK", ProviderVersion, Compatibility.GetObjectField("provider_sdk"), Logger);
		ValidateVersion("adapter", AdapterVersion, Compatibility.GetObjectField("adapter"), Logger);
		ValidateVersion("network SDK", NetworkVersion, Compatibility.GetObjectField("network_sdk"), Logger);
		return new[] { AdapterManifestPath, AdapterPackagesPath, ProviderPackagesPath };
	}

	/** Links Unity Ads mediation, metadata code, and privacy resources only for enabled iOS targets. */
	public OpenMobileAdsAdMobUnityIOS(ReadOnlyTargetRules Target) : base(Target)
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
			"UnityAdapter",
			Path.Combine(ThirdPartyIOSPath, "UnityAdapter.xcframework.zip"),
			Framework.FrameworkMode.Link
		));
		PublicAdditionalFrameworks.Add(new Framework(
			"UnityAds",
			Path.Combine(ThirdPartyIOSPath, "UnityAds.xcframework.zip"),
			Framework.FrameworkMode.Link
		));

		string PrivacyBundlePath = Path.Combine(
			ModuleDirectory,
			"../../Resources/IOS/OpenMobileAdsAdMobUnityPrivacy.bundle"
		);
		AdditionalBundleResources.Add(new BundleResource(PrivacyBundlePath, ""));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "Info.plist"));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "PrivacyInfo.xcprivacy"));

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/IOS/OpenMobileAdsAdMobUnity_IOS_UPL.xml")
		);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileAdsAdMobUnity_IOS_UPL.xml")
		);
	}
}

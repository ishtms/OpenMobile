using System;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

public class OpenMobileAdsAdMobMetaIOS : ModuleRules
{
	/** Reads pinned Meta, adapter, and provider versions from their package manifests. */
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
			$"OpenMobile Ads AdMob Meta could not find package '{PackageName}' in '{ManifestPath}'."
		);
	}

	/** Rejects incompatible Meta versions and warns about supported combinations not yet exercised. */
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
				"OpenMobile Ads AdMob Meta could not parse the {Name} compatibility values.",
				Name
			);
			return;
		}
		if (ActualVersion < MinimumVersion || ActualVersion >= MaximumVersion)
		{
			throw new BuildException(
				$"OpenMobile Ads AdMob Meta {Name} {Actual} is outside supported range " +
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
				"OpenMobile Ads AdMob Meta {Name} {Actual} is supported but not tested.",
				Name,
				Actual
			);
		}
	}

	/** Checks provider, Meta adapter, and Audience Network versions before adding frameworks. */
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
			"Google Mobile Ads Mediation Adapter for Meta"
		);
		string NetworkVersion = ReadPackageVersion(
			AdapterPackagesPath,
			"Meta Audience Network SDK for iOS"
		);
		if (AdapterVersion != IOS.GetStringField("adapter_version"))
		{
			throw new BuildException("OpenMobile Ads AdMob Meta adapter metadata conflicts with its iOS package manifest.");
		}
		if (NetworkVersion != IOS.GetStringField("network_sdk_version"))
		{
			throw new BuildException("OpenMobile Ads AdMob Meta network metadata conflicts with its iOS package manifest.");
		}

		ValidateVersion("provider SDK", ProviderVersion, Compatibility.GetObjectField("provider_sdk"), Logger);
		ValidateVersion("adapter", AdapterVersion, Compatibility.GetObjectField("adapter"), Logger);
		ValidateVersion("network SDK", NetworkVersion, Compatibility.GetObjectField("network_sdk"), Logger);
		return new[] { AdapterManifestPath, AdapterPackagesPath, ProviderPackagesPath };
	}

	/** Links Meta Audience Network and its pre-initialization participant only for opted-in iOS builds. */
	public OpenMobileAdsAdMobMetaIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		ExternalDependencies.AddRange(ValidateCompatibility(ModuleDirectory, Target.Logger));

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileAdsAdMob",
			"Swift"
		});

		PublicFrameworks.AddRange(new[]
		{
			"AppTrackingTransparency",
			"AudioToolbox",
			"AVFoundation",
			"CoreGraphics",
			"CoreMedia",
			"CoreTelephony",
			"Foundation",
			"StoreKit",
			"UIKit",
			"WebKit"
		});

		string ThirdPartyIOSPath = Path.Combine(ModuleDirectory, "../../ThirdParty/IOS");
		PublicAdditionalFrameworks.Add(new Framework(
			"MetaAdapter",
			Path.Combine(ThirdPartyIOSPath, "MetaAdapter.xcframework.zip"),
			Framework.FrameworkMode.Link
		));
		PublicAdditionalFrameworks.Add(new Framework(
			"FBAudienceNetwork",
			Path.Combine(ThirdPartyIOSPath, "FBAudienceNetwork.xcframework.zip"),
			Framework.FrameworkMode.LinkAndCopy
		));

		string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
		ExternalDependencies.Add(
			Path.Combine(ModuleDirectory, "Private/IOS/OpenMobileAdsAdMobMeta_IOS_UPL.xml")
		);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileAdsAdMobMeta_IOS_UPL.xml")
		);
	}
}

using System.IO;
using UnrealBuildTool;

public class OpenMobileSensorsIOS : ModuleRules
{
	public OpenMobileSensorsIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bEnableObjCAutomaticReferenceCounting = true;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobilePermissions",
			"OpenMobileSensors"
		});

		PublicFrameworks.AddRange(new[]
		{
			"CoreMotion",
			"Foundation",
			"UIKit"
		});

		string PrivacyBundlePath = Path.Combine(
			ModuleDirectory,
			"../../Resources/IOS/OpenMobileSensorsPrivacy.bundle"
		);
		AdditionalBundleResources.Add(new BundleResource(PrivacyBundlePath, ""));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "Info.plist"));
		ExternalDependencies.Add(Path.Combine(PrivacyBundlePath, "PrivacyInfo.xcprivacy"));

		string ModulePath = Utils.MakePathRelativeTo(
			ModuleDirectory,
			Target.RelativeEnginePath
		);
		string IOSPluginPath = Path.Combine(
			ModuleDirectory,
			"Private/IOS/OpenMobileSensors_IOS_UPL.xml"
		);
		ExternalDependencies.Add(IOSPluginPath);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileSensors_IOS_UPL.xml")
		);
	}
}

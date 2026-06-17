using System.IO;
using UnrealBuildTool;

public class OpenMobileDeviceIOS : ModuleRules
{
	public OpenMobileDeviceIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"Core",
			"OpenMobileDevice",
			"RHI"
		});

		PublicFrameworks.AddRange(new[]
		{
			"AVFoundation",
			"Foundation",
			"SystemConfiguration",
			"UIKit"
		});

		string ModulePath = Utils.MakePathRelativeTo(
			ModuleDirectory,
			Target.RelativeEnginePath
		);
		string IOSPluginPath = Path.Combine(
			ModuleDirectory,
			"Private/IOS/OpenMobileDevice_IOS_UPL.xml"
		);
		ExternalDependencies.Add(IOSPluginPath);
		AdditionalPropertiesForReceipt.Add(
			"IOSPlugin",
			Path.Combine(ModulePath, "Private/IOS/OpenMobileDevice_IOS_UPL.xml")
		);
	}
}

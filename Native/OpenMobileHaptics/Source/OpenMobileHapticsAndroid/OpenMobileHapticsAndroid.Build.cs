using System.IO;
using UnrealBuildTool;

public class OpenMobileHapticsAndroid : ModuleRules
{
	public OpenMobileHapticsAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"Launch",
			"OpenMobileHaptics"
		});

		PrivateIncludePathModuleNames.Add("Launch");

		string ModulePath = Utils.MakePathRelativeTo(
			ModuleDirectory,
			Target.RelativeEnginePath
		);
		AdditionalPropertiesForReceipt.Add(
			"AndroidPlugin",
			Path.Combine(
				ModulePath,
				"Private/Android/OpenMobileHaptics_Android_UPL.xml"
			)
		);
	}
}

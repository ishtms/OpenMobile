using System.IO;
using UnrealBuildTool;

public class OpenMobileSensorsAndroid : ModuleRules
{
	public OpenMobileSensorsAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"Launch",
			"OpenMobileSensors"
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
				"Private/Android/OpenMobileSensors_Android_UPL.xml"
			)
		);
	}
}

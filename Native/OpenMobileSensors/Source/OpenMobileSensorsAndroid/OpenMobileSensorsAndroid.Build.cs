using UnrealBuildTool;

public class OpenMobileSensorsAndroid : ModuleRules
{
	public OpenMobileSensorsAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileSensors"
		});
	}
}

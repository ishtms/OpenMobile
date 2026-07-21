using UnrealBuildTool;

public class OpenMobileSensorsSensorsOnly : ModuleRules
{
	public OpenMobileSensorsSensorsOnly(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileSensors"
		});
	}
}

using UnrealBuildTool;

public class OpenMobileSensorsSensorsActivityProvider : ModuleRules
{
	public OpenMobileSensorsSensorsActivityProvider(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileSensors"
		});
	}
}

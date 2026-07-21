using UnrealBuildTool;

public class OpenMobileSensorsSensorsPermissions : ModuleRules
{
	public OpenMobileSensorsSensorsPermissions(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobilePermissions",
			"OpenMobileSensors"
		});
	}
}

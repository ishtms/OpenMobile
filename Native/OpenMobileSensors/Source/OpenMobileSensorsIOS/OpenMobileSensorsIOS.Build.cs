using UnrealBuildTool;

public class OpenMobileSensorsIOS : ModuleRules
{
	public OpenMobileSensorsIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileSensors"
		});
	}
}

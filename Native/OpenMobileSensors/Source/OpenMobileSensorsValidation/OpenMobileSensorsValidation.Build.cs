using UnrealBuildTool;

public class OpenMobileSensorsValidation : ModuleRules
{
	public OpenMobileSensorsValidation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileSensors"
		});
	}
}

using UnrealBuildTool;

public class OpenMobileSensorsActivityProviderFixture : ModuleRules
{
	public OpenMobileSensorsActivityProviderFixture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileSensors"
		});
	}
}

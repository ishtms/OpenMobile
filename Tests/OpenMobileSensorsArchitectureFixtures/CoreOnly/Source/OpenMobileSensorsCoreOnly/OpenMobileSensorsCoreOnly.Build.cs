using UnrealBuildTool;

public class OpenMobileSensorsCoreOnly : ModuleRules
{
	public OpenMobileSensorsCoreOnly(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileCore"
		});
	}
}

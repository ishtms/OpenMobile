using UnrealBuildTool;

public class OpenMobileSensors : ModuleRules
{
	public OpenMobileSensors(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileCore",
			"OpenMobilePermissions"
		});
	}
}

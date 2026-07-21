using UnrealBuildTool;

public class OpenMobileSensorsSampleHost : ModuleRules
{
	public OpenMobileSensorsSampleHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileCore",
			"OpenMobilePermissions",
			"OpenMobileSensors",
			"UMG"
		});
	}
}

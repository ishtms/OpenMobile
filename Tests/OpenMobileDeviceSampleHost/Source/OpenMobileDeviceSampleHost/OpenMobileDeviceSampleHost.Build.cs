using UnrealBuildTool;

public class OpenMobileDeviceSampleHost : ModuleRules
{
	public OpenMobileDeviceSampleHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileCore",
			"OpenMobileDevice",
			"UMG"
		});
	}
}

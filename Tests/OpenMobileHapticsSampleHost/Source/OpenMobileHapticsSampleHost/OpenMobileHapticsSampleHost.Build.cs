using UnrealBuildTool;

public class OpenMobileHapticsSampleHost : ModuleRules
{
	public OpenMobileHapticsSampleHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileCore",
			"OpenMobileHaptics",
			"UMG"
		});
	}
}

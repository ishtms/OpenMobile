using UnrealBuildTool;

public class OpenMobileHapticsUMG : ModuleRules
{
	public OpenMobileHapticsUMG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"OpenMobileHaptics",
			"SlateCore",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Engine",
			"Slate"
		});
	}
}

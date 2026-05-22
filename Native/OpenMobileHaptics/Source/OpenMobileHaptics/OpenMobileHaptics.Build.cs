using UnrealBuildTool;

public class OpenMobileHaptics : ModuleRules
{
	public OpenMobileHaptics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileCore"
		});
	}
}

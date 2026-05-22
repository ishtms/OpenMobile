using UnrealBuildTool;

public class OpenMobileHapticsEditor : ModuleRules
{
	public OpenMobileHapticsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"OpenMobileHaptics"
		});
	}
}

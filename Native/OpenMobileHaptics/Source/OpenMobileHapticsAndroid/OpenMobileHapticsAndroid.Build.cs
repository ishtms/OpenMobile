using UnrealBuildTool;

public class OpenMobileHapticsAndroid : ModuleRules
{
	public OpenMobileHapticsAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileHaptics"
		});
	}
}

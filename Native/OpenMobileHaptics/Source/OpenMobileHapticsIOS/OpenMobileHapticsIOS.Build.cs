using UnrealBuildTool;

public class OpenMobileHapticsIOS : ModuleRules
{
	public OpenMobileHapticsIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileHaptics"
		});

		PublicFrameworks.AddRange(new[]
		{
			"CoreHaptics",
			"Foundation"
		});
	}
}

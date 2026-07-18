using UnrealBuildTool;

public class OpenMobileHapticsSequencer : ModuleRules
{
	public OpenMobileHapticsSequencer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"MovieScene",
			"OpenMobileHaptics"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Engine",
			"MovieSceneTracks"
		});
	}
}

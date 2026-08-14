using UnrealBuildTool;

public class OpenMobileHaptics : ModuleRules
{
	/** Keeps the runtime module platform-neutral, editor-only target support is added only when the target can use it. */
	public OpenMobileHaptics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"Json",
			"OpenMobileCore"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("TargetPlatform");
		}
	}
}

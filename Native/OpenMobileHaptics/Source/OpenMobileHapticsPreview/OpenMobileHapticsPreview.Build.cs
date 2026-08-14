using UnrealBuildTool;

public class OpenMobileHapticsPreview : ModuleRules
{
	/** Enables device preview only in Development targets while keeping protocol types available to editor code. */
	public OpenMobileHapticsPreview(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileHaptics"
		});
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Json",
			"Networking",
			"Sockets"
		});
		PublicDefinitions.Add(
			"OPENMOBILE_HAPTICS_PREVIEW_ENABLED="
			+ (Target.Configuration == UnrealTargetConfiguration.Development
				? "1" : "0")
		);
	}
}

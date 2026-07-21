using UnrealBuildTool;

public class OpenMobileHapticsPreview : ModuleRules
{
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

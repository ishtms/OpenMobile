using UnrealBuildTool;

public class OpenMobileDevice : ModuleRules
{
	public OpenMobileDevice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"OpenMobileCore"
		});
	}
}

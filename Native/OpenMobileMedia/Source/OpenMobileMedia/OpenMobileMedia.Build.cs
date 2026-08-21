using UnrealBuildTool;

public class OpenMobileMedia : ModuleRules
{
	public OpenMobileMedia(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileCore"
		});

		PrivateDependencyModuleNames.Add("Json");
	}
}

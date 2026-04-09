using UnrealBuildTool;

public class OpenMobileAds : ModuleRules
{
	public OpenMobileAds(ReadOnlyTargetRules Target) : base(Target)
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

using UnrealBuildTool;

public class OpenMobileCore : ModuleRules
{
	public OpenMobileCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject"
		});
	}
}

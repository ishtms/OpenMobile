using UnrealBuildTool;

public class OpenMobilePermissions : ModuleRules
{
	public OpenMobilePermissions(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"OpenMobileCore"
		});
	}
}

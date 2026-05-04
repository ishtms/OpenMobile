using UnrealBuildTool;

public class OpenMobileDeviceEditor : ModuleRules
{
	public OpenMobileDeviceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileDevice"
		});
	}
}

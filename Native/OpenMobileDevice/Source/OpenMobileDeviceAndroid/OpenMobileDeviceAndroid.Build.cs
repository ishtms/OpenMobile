using UnrealBuildTool;

public class OpenMobileDeviceAndroid : ModuleRules
{
	public OpenMobileDeviceAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"Core",
			"OpenMobileDevice"
		});
	}
}

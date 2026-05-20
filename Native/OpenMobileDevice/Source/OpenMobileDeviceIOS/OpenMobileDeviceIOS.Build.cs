using UnrealBuildTool;

public class OpenMobileDeviceIOS : ModuleRules
{
	public OpenMobileDeviceIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"Core",
			"OpenMobileDevice"
		});

		PublicFrameworks.AddRange(new[]
		{
			"Foundation",
			"SystemConfiguration",
			"UIKit"
		});
	}
}

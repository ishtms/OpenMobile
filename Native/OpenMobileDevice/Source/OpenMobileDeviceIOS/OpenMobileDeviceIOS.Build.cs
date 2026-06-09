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
			"OpenMobileDevice",
			"RHI"
		});

		PublicFrameworks.AddRange(new[]
		{
			"AVFoundation",
			"Foundation",
			"SystemConfiguration",
			"UIKit"
		});
	}
}

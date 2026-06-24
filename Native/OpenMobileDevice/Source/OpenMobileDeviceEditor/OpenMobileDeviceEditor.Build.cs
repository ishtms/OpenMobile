using UnrealBuildTool;

public class OpenMobileDeviceEditor : ModuleRules
{
	public OpenMobileDeviceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"OpenMobileCore",
			"OpenMobileDevice"
		});

		PrivateDependencyModuleNames.Add("UnrealEd");
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"DesktopPlatform",
			"Json",
			"Projects",
			"Slate",
			"SlateCore"
		});
	}
}

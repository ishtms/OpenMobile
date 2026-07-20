using UnrealBuildTool;

public class OpenMobileSensorsEditor : ModuleRules
{
	public OpenMobileSensorsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"BlueprintGraph",
			"Core",
			"CoreUObject",
			"DesktopPlatform",
			"DeveloperSettings",
			"Engine",
			"Json",
			"Kismet",
			"MessageLog",
			"OpenMobileCore",
			"OpenMobilePermissions",
			"OpenMobileSensors",
			"Projects",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}

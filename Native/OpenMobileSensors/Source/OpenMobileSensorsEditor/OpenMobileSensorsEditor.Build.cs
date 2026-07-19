using UnrealBuildTool;

public class OpenMobileSensorsEditor : ModuleRules
{
	public OpenMobileSensorsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"BlueprintGraph",
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"Kismet",
			"MessageLog",
			"OpenMobilePermissions",
			"OpenMobileSensors",
			"Projects",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}

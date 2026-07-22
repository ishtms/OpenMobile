using UnrealBuildTool;

public class OpenMobileHapticsEditor : ModuleRules
{
	public OpenMobileHapticsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"AssetDefinition",
			"AssetRegistry",
			"AssetTools",
			"Core",
			"CoreUObject",
			"EditorFramework",
			"Engine",
			"InputCore",
			"Json",
			"OpenMobileCore",
			"OpenMobileHaptics",
			"OpenMobileHapticsPreview",
			"PropertyEditor",
			"Projects",
			"Slate",
			"SlateCore",
			"Sockets",
			"Networking",
			"ToolMenus",
			"UnrealEd"
		});
	}
}

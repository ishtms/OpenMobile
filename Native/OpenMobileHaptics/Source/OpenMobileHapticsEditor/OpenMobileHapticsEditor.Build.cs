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
			"AssetTools",
			"Core",
			"CoreUObject",
			"EditorFramework",
			"Engine",
			"InputCore",
			"Json",
			"OpenMobileHaptics",
			"OpenMobileHapticsPreview",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"Sockets",
			"Networking",
			"ToolMenus",
			"UnrealEd"
		});
	}
}

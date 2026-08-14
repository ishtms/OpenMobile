using UnrealBuildTool;

public class OpenMobileHapticsEditor : ModuleRules
{
	/** Keeps asset editing, Blueprint checks, diagnostics, Slate, and preview transport out of packaged runtime modules. */
	public OpenMobileHapticsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"AssetDefinition",
			"AssetRegistry",
			"AssetTools",
			"BlueprintGraph",
			"Core",
			"CoreUObject",
			"EditorFramework",
			"Engine",
			"InputCore",
			"Json",
			"Kismet",
			"KismetCompiler",
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

using UnrealBuildTool;

public class OpenMobileAdsEditor : ModuleRules
{
	/** Keeps settings validation, details customization, and Message Log support outside packaged Ads runtime. */
	public OpenMobileAdsEditor(ReadOnlyTargetRules Target) : base(Target)
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
			"KismetCompiler",
			"MessageLog",
			"OpenMobileAds",
			"PropertyEditor",
			"Projects",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}

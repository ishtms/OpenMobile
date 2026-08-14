using UnrealBuildTool;

public class OpenMobileAdsEditor : ModuleRules
{
	/** Keeps settings validation, details customization, and Message Log support outside packaged Ads runtime. */
	public OpenMobileAdsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"MessageLog",
			"OpenMobileAds",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}

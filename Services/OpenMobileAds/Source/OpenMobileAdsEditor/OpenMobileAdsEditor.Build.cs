using UnrealBuildTool;

public class OpenMobileAdsEditor : ModuleRules
{
	public OpenMobileAdsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"MessageLog",
			"OpenMobileAds",
			"UnrealEd"
		});
	}
}

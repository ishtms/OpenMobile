using UnrealBuildTool;

public class OpenMobileAdsAdMobEditor : ModuleRules
{
	public OpenMobileAdsAdMobEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"MessageLog",
			"OpenMobileAdsAdMob",
			"UnrealEd"
		});
	}
}

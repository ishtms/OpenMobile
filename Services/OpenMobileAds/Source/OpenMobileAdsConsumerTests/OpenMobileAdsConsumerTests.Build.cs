using UnrealBuildTool;

public class OpenMobileAdsConsumerTests : ModuleRules
{
	public OpenMobileAdsConsumerTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"OpenMobileAds"
		});
	}
}

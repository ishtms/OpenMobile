using UnrealBuildTool;

public class OpenMobileAdsConsumerTests : ModuleRules
{
	/** Compiles public Ads contracts without granting the consumer module private include access. */
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

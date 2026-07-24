using UnrealBuildTool;

public class OpenMobileHapticsSampleHost : ModuleRules
{
	public OpenMobileHapticsSampleHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileCore",
			"OpenMobileHaptics",
			"UMG"
		});

		bool bEnableCapabilitySnapshot =
			Target.Configuration == UnrealTargetConfiguration.Development;
		if (bEnableCapabilitySnapshot)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"ApplicationCore",
				"OpenMobileHapticsPreview"
			});
		}
		PublicDefinitions.Add(
			"OPENMOBILE_HAPTICS_SAMPLE_SNAPSHOT_ENABLED="
			+ (bEnableCapabilitySnapshot ? "1" : "0")
		);
	}
}

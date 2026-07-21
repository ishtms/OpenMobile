using UnrealBuildTool;

public class OpenMobileHapticsUMGTester : ModuleRules
{
	public OpenMobileHapticsUMGTester(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"OpenMobileHaptics",
			"OpenMobileHapticsPreview",
			"UMG"
		});
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"Engine",
			"Slate",
			"SlateCore"
		});
		PublicDefinitions.Add(
			"OPENMOBILE_HAPTICS_TESTER_ENABLED="
			+ (Target.Configuration == UnrealTargetConfiguration.Development
				? "1" : "0")
		);
	}
}

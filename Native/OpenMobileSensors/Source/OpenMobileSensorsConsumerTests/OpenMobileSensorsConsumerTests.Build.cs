using UnrealBuildTool;

public class OpenMobileSensorsConsumerTests : ModuleRules
{
	public OpenMobileSensorsConsumerTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OpenMobileCore",
			"OpenMobilePermissions",
			"OpenMobileSensors"
		});
	}
}

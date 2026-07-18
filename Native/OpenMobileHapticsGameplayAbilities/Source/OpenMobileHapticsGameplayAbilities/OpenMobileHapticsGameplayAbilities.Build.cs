using UnrealBuildTool;

public class OpenMobileHapticsGameplayAbilities : ModuleRules
{
	public OpenMobileHapticsGameplayAbilities(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"GameplayAbilities",
			"OpenMobileHaptics"
		});

		PrivateDependencyModuleNames.Add("Engine");
	}
}
